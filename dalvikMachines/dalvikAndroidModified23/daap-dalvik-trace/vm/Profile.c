/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Android's method call profiling goodies.
 */
#include "Dalvik.h"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <sys/mman.h>
#include <sched.h>
#include <errno.h>
#include <fcntl.h>

#include <cutils/open_memstream.h>

#ifdef HAVE_ANDROID_OS
# define UPDATE_MAGIC_PAGE      1
#endif

#define DMTRACE_ENABLED 0
#define LOGD_TRACE_ENABLED 1

/*
 * File format:
 *  header
 *  record 0
 *  record 1
 *  ...
 *
 * Header format:
 *  u4  magic ('SLOW')
 *  u2  version
 *  u2  offset to data
 *  u8  start date/time in usec
 *
 * Record format:
 *  u1  thread ID
 *  u4  method ID | method action
 *  u4  time delta since start, in usec
 *
 * 32 bits of microseconds is 70 minutes.
 *
 * All values are stored in little-endian order.
 */
#define TRACE_REC_SIZE      9
#define TRACE_MAGIC         0x574f4c53
#define TRACE_HEADER_LEN    32

#define FILL_PATTERN        0xeeeeeeee

/*
 * Get the wall-clock date/time, in usec.
 */
static inline u8 getTimeInUsec()
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

/*
 * Get the current time, in microseconds.
 *
 * This can mean one of two things.  In "global clock" mode, we get the
 * same time across all threads.  If we use CLOCK_THREAD_CPUTIME_ID, we
 * get a per-thread CPU usage timer.  The latter is better, but a bit
 * more complicated to implement.
 */
static inline u8 getClock()
{
#if defined(HAVE_POSIX_CLOCKS)
    if (!gDvm.profilerWallClock) {
        struct timespec tm;

        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tm);
        if (!(tm.tv_nsec >= 0 && tm.tv_nsec < 1*1000*1000*1000)) {
            LOGE("bad nsec: %ld\n", tm.tv_nsec);
            dvmAbort();
        }

        return tm.tv_sec * 1000000LL + tm.tv_nsec / 1000;
    } else
#endif
    {
        struct timeval tv;

        gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000000LL + tv.tv_usec;
    }
}

/*
 * Write little-endian data.
 */
static inline void storeShortLE(u1* buf, u2 val)
{
    *buf++ = (u1) val;
    *buf++ = (u1) (val >> 8);
}
static inline void storeIntLE(u1* buf, u4 val)
{
    *buf++ = (u1) val;
    *buf++ = (u1) (val >> 8);
    *buf++ = (u1) (val >> 16);
    *buf++ = (u1) (val >> 24);
}
static inline void storeLongLE(u1* buf, u8 val)
{
    *buf++ = (u1) val;
    *buf++ = (u1) (val >> 8);
    *buf++ = (u1) (val >> 16);
    *buf++ = (u1) (val >> 24);
    *buf++ = (u1) (val >> 32);
    *buf++ = (u1) (val >> 40);
    *buf++ = (u1) (val >> 48);
    *buf++ = (u1) (val >> 56);
}

/*
 * Boot-time init.
 */
bool dvmProfilingStartup(void)
{
    /*
     * Initialize "dmtrace" method profiling.
     */
    memset(&gDvm.methodTrace, 0, sizeof(gDvm.methodTrace));
    dvmInitMutex(&gDvm.methodTrace.startStopLock);
    pthread_cond_init(&gDvm.methodTrace.threadExitCond, NULL);

    ClassObject* clazz =
        dvmFindClassNoInit("Ldalvik/system/VMDebug;", NULL);
    assert(clazz != NULL);
    gDvm.methodTrace.gcMethod =
        dvmFindDirectMethodByDescriptor(clazz, "startGC", "()V");
    gDvm.methodTrace.classPrepMethod =
        dvmFindDirectMethodByDescriptor(clazz, "startClassPrep", "()V");
    if (gDvm.methodTrace.gcMethod == NULL ||
        gDvm.methodTrace.classPrepMethod == NULL)
    {
        LOGE("Unable to find startGC or startClassPrep\n");
        return false;
    }

    assert(!dvmCheckException(dvmThreadSelf()));

    /*
     * Allocate storage for instruction counters.
     */
    gDvm.executedInstrCounts = (int*) malloc(kNumDalvikInstructions * sizeof(int));
    if (gDvm.executedInstrCounts == NULL)
        return false;
    memset(gDvm.executedInstrCounts, 0, kNumDalvikInstructions * sizeof(int));

#ifdef UPDATE_MAGIC_PAGE
    /*
     * If we're running on the emulator, there's a magic page into which
     * we can put interpreted method information.  This allows interpreted
     * methods to show up in the emulator's code traces.
     *
     * We could key this off of the "ro.kernel.qemu" property, but there's
     * no real harm in doing this on a real device.
     */
    int fd = open("/dev/qemu_trace", O_RDWR);
    if (fd < 0) {
        LOGV("Unable to open /dev/qemu_trace\n");
    } else {
        gDvm.emulatorTracePage = mmap(0, SYSTEM_PAGE_SIZE, PROT_READ|PROT_WRITE,
                                      MAP_SHARED, fd, 0);
        close(fd);
        if (gDvm.emulatorTracePage == MAP_FAILED) {
            LOGE("Unable to mmap /dev/qemu_trace\n");
            gDvm.emulatorTracePage = NULL;
        } else {
            *(u4*) gDvm.emulatorTracePage = 0;
        }
    }
#else
    assert(gDvm.emulatorTracePage == NULL);
#endif

    return true;
}

/*
 * Free up profiling resources.
 */
void dvmProfilingShutdown(void)
{
#ifdef UPDATE_MAGIC_PAGE
    if (gDvm.emulatorTracePage != NULL)
        munmap(gDvm.emulatorTracePage, SYSTEM_PAGE_SIZE);
#endif
    free(gDvm.executedInstrCounts);
}

/*
 * Update the "active profilers" count.
 *
 * "count" should be +1 or -1.
 */
static void updateActiveProfilers(int count)
{
    int oldValue, newValue;

    do {
        oldValue = gDvm.activeProfilers;
        newValue = oldValue + count;
        if (newValue < 0) {
            LOGE("Can't have %d active profilers\n", newValue);
            dvmAbort();
        }
    } while (android_atomic_release_cas(oldValue, newValue,
            &gDvm.activeProfilers) != 0);

    LOGD("+++ active profiler count now %d\n", newValue);
#if defined(WITH_JIT)
    dvmCompilerStateRefresh();
#endif
}


/*
 * Reset the "cpuClockBase" field in all threads.
 */
static void resetCpuClockBase(void)
{
    Thread* thread;

    dvmLockThreadList(NULL);
    for (thread = gDvm.threadList; thread != NULL; thread = thread->next) {
        thread->cpuClockBaseSet = false;
        thread->cpuClockBase = 0;
    }
    dvmUnlockThreadList();
}

/*
 * Dump the thread list to the specified file.
 */
static void dumpThreadList(FILE* fp)
{
    Thread* thread;

    dvmLockThreadList(NULL);
    for (thread = gDvm.threadList; thread != NULL; thread = thread->next) {
        char* name = dvmGetThreadName(thread);

        fprintf(fp, "%d\t%s\n", thread->threadId, name);
        free(name);
    }
    dvmUnlockThreadList();
}

/*
 * This is a dvmHashForeach callback.
 */
static int dumpMarkedMethods(void* vclazz, void* vfp)
{
    DexStringCache stringCache;
    ClassObject* clazz = (ClassObject*) vclazz;
    FILE* fp = (FILE*) vfp;
    Method* meth;
    char* name;
    int i;

    dexStringCacheInit(&stringCache);

    for (i = 0; i < clazz->virtualMethodCount; i++) {
        meth = &clazz->virtualMethods[i];
        if (meth->inProfile) {
            name = dvmDescriptorToName(meth->clazz->descriptor);
            fprintf(fp, "0x%08x\t%s\t%s\t%s\t%s\t%d\n", (int) meth,
                name, meth->name,
                dexProtoGetMethodDescriptor(&meth->prototype, &stringCache),
                dvmGetMethodSourceFile(meth), dvmLineNumFromPC(meth, 0));
            meth->inProfile = false;
            free(name);
        }
    }

    for (i = 0; i < clazz->directMethodCount; i++) {
        meth = &clazz->directMethods[i];
        if (meth->inProfile) {
            name = dvmDescriptorToName(meth->clazz->descriptor);
            fprintf(fp, "0x%08x\t%s\t%s\t%s\t%s\t%d\n", (int) meth,
                name, meth->name,
                dexProtoGetMethodDescriptor(&meth->prototype, &stringCache),
                dvmGetMethodSourceFile(meth), dvmLineNumFromPC(meth, 0));
            meth->inProfile = false;
            free(name);
        }
    }

    dexStringCacheRelease(&stringCache);

    return 0;
}

/*
 * Dump the list of "marked" methods to the specified file.
 */
static void dumpMethodList(FILE* fp)
{
    dvmHashTableLock(gDvm.loadedClasses);
    dvmHashForeach(gDvm.loadedClasses, dumpMarkedMethods, (void*) fp);
    dvmHashTableUnlock(gDvm.loadedClasses);
}

/*
 * Start method tracing.  Method tracing is global to the VM (i.e. we
 * trace all threads).
 *
 * This opens the output file (if an already open fd has not been supplied,
 * and we're not going direct to DDMS) and allocates the data buffer.  This
 * takes ownership of the file descriptor, closing it on completion.
 *
 * On failure, we throw an exception and return.
 */
void dvmMethodTraceStart(const char* traceFileName, int traceFd, int bufferSize,
    int flags, bool directToDdms)
{
    MethodTraceState* state = &gDvm.methodTrace;

    assert(bufferSize > 0);

    dvmLockMutex(&state->startStopLock);
    while (state->traceEnabled != 0) {
        LOGI("TRACE start requested, but already in progress; stopping\n");
        dvmUnlockMutex(&state->startStopLock);
        dvmMethodTraceStop();
        dvmLockMutex(&state->startStopLock);
    }
    updateActiveProfilers(1);
    LOGI("TRACE STARTED: '%s' %dKB\n", traceFileName, bufferSize / 1024);

    /*
     * Allocate storage and open files.
     *
     * We don't need to initialize the buffer, but doing so might remove
     * some fault overhead if the pages aren't mapped until touched.
     */
    state->buf = (u1*) malloc(bufferSize);
    if (state->buf == NULL) {
        dvmThrowException("Ljava/lang/InternalError;", "buffer alloc failed");
        goto fail;
    }
    if (!directToDdms) {
        if (traceFd < 0) {
            state->traceFile = fopen(traceFileName, "w");
        } else {
            state->traceFile = fdopen(traceFd, "w");
        }
        if (state->traceFile == NULL) {
            int err = errno;
            LOGE("Unable to open trace file '%s': %s\n",
                traceFileName, strerror(err));
            dvmThrowExceptionFmt("Ljava/lang/RuntimeException;",
                "Unable to open trace file '%s': %s",
                traceFileName, strerror(err));
            goto fail;
        }
    }
    traceFd = -1;
    memset(state->buf, (char)FILL_PATTERN, bufferSize);

    state->directToDdms = directToDdms;
    state->bufferSize = bufferSize;
    state->overflow = false;

    /*
     * Enable alloc counts if we've been requested to do so.
     */
    state->flags = flags;
    if ((flags & TRACE_ALLOC_COUNTS) != 0)
        dvmStartAllocCounting();

    /* reset our notion of the start time for all CPU threads */
    resetCpuClockBase();

    state->startWhen = getTimeInUsec();

    /*
     * Output the header.
     */
    memset(state->buf, 0, TRACE_HEADER_LEN);
    storeIntLE(state->buf + 0, TRACE_MAGIC);
    storeShortLE(state->buf + 4, TRACE_VERSION);
    storeShortLE(state->buf + 6, TRACE_HEADER_LEN);
    storeLongLE(state->buf + 8, state->startWhen);
    state->curOffset = TRACE_HEADER_LEN;

    /*
     * Set the "enabled" flag.  Once we do this, threads will wait to be
     * signaled before exiting, so we have to make sure we wake them up.
     */
    android_atomic_release_store(true, &state->traceEnabled);
    dvmUnlockMutex(&state->startStopLock);
    return;

fail:
    updateActiveProfilers(-1);
    if (state->traceFile != NULL) {
        fclose(state->traceFile);
        state->traceFile = NULL;
    }
    if (state->buf != NULL) {
        free(state->buf);
        state->buf = NULL;
    }
    if (traceFd >= 0)
        close(traceFd);
    dvmUnlockMutex(&state->startStopLock);
}

/*
 * Run through the data buffer and pull out the methods that were visited.
 * Set a mark so that we know which ones to output.
 */
static void markTouchedMethods(int endOffset)
{
    u1* ptr = gDvm.methodTrace.buf + TRACE_HEADER_LEN;
    u1* end = gDvm.methodTrace.buf + endOffset;
    unsigned int methodVal;
    Method* method;

    while (ptr < end) {
        methodVal = *(ptr+1) | (*(ptr+2) << 8) | (*(ptr+3) << 16)
                    | (*(ptr+4) << 24);
        method = (Method*) METHOD_ID(methodVal);

        method->inProfile = true;
        ptr += TRACE_REC_SIZE;
    }
}

/*
 * Compute the amount of overhead in a clock call, in nsec.
 *
 * This value is going to vary depending on what else is going on in the
 * system.  When examined across several runs a pattern should emerge.
 */
static u4 getClockOverhead(void)
{
    u8 calStart, calElapsed;
    int i;

    calStart = getClock();
    for (i = 1000 * 4; i > 0; i--) {
        getClock();
        getClock();
        getClock();
        getClock();
        getClock();
        getClock();
        getClock();
        getClock();
    }

    calElapsed = getClock() - calStart;
    return (int) (calElapsed / (8*4));
}

/*
 * Returns "true" if method tracing is currently active.
 */
bool dvmIsMethodTraceActive(void)
{
    const MethodTraceState* state = &gDvm.methodTrace;
    return state->traceEnabled;
}

/*
 * Stop method tracing.  We write the buffer to disk and generate a key
 * file so we can interpret it.
 */
void dvmMethodTraceStop(void)
{
    MethodTraceState* state = &gDvm.methodTrace;
    u8 elapsed;

    /*
     * We need this to prevent somebody from starting a new trace while
     * we're in the process of stopping the old.
     */
    dvmLockMutex(&state->startStopLock);

    if (!state->traceEnabled) {
        /* somebody already stopped it, or it was never started */
        LOGD("TRACE stop requested, but not running\n");
        dvmUnlockMutex(&state->startStopLock);
        return;
    } else {
        updateActiveProfilers(-1);
    }

    /* compute elapsed time */
    elapsed = getTimeInUsec() - state->startWhen;

    dvmLockMutex(&state->fwriteLock);
    Thread *thread;
    for (thread = gDvm.threadList; thread != NULL; thread = thread->next) {
        if (thread->dump != NULL) {
            LOGD("closing method trace output @ %p via dvmMethodTraceStop()\n", thread->dump);
            fclose(thread->dump);
            thread->dump = NULL;
        }
    }
    /*
     * Globally disable it, and allow other threads to notice.  We want
     * to stall here for at least as long as dvmMethodTraceAdd needs
     * to finish.  There's no real risk though -- it will take a while to
     * write the data to disk, and we don't clear the buffer pointer until
     * after that completes.
     */
    state->traceEnabled = false;
    dvmUnlockMutex(&state->fwriteLock);

    ANDROID_MEMBAR_FULL();
    sched_yield();
    usleep(250 * 1000);

    if ((state->flags & TRACE_ALLOC_COUNTS) != 0)
        dvmStopAllocCounting();

    /*
     * It's possible under some circumstances for a thread to have advanced
     * the data pointer but not written the method value.  It's possible
     * (though less likely) for the data pointer to be advanced, or partial
     * data written, while we're doing work here.
     *
     * To avoid seeing partially-written data, we grab state->curOffset here,
     * and use our local copy from here on.  We then scan through what's
     * already written.  If we see the fill pattern in what should be the
     * method pointer, we cut things off early.  (If we don't, we'll fail
     * when we dereference the pointer.)
     *
     * There's a theoretical possibility of interrupting another thread
     * after it has partially written the method pointer, in which case
     * we'll likely crash when we dereference it.  The possibility of
     * this actually happening should be at or near zero.  Fixing it
     * completely could be done by writing the thread number last and
     * using a sentinel value to indicate a partially-written record,
     * but that requires memory barriers.
     */
    int finalCurOffset = state->curOffset;

    if (finalCurOffset > TRACE_HEADER_LEN) {
        u4 fillVal = METHOD_ID(FILL_PATTERN);
        u1* scanPtr = state->buf + TRACE_HEADER_LEN;

        while (scanPtr < state->buf + finalCurOffset) {
            u4 methodVal = scanPtr[1] | (scanPtr[2] << 8) | (scanPtr[3] << 16)
                        | (scanPtr[4] << 24);
            if (METHOD_ID(methodVal) == fillVal) {
                u1* scanBase = state->buf + TRACE_HEADER_LEN;
                LOGW("Found unfilled record at %d (of %d)\n",
                    (scanPtr - scanBase) / TRACE_REC_SIZE,
                    (finalCurOffset - TRACE_HEADER_LEN) / TRACE_REC_SIZE);
                finalCurOffset = scanPtr - state->buf;
                break;
            }

            scanPtr += TRACE_REC_SIZE;
        }
    }

    LOGI("TRACE STOPPED%s: writing %d records\n",
        state->overflow ? " (NOTE: overflowed buffer)" : "",
        (finalCurOffset - TRACE_HEADER_LEN) / TRACE_REC_SIZE);
    if (gDvm.debuggerActive) {
        LOGW("WARNING: a debugger is active; method-tracing results "
             "will be skewed\n");
    }

    /*
     * Do a quick calibration test to see how expensive our clock call is.
     */
    u4 clockNsec = getClockOverhead();

    markTouchedMethods(finalCurOffset);

    char* memStreamPtr;
    size_t memStreamSize;
    if (state->directToDdms) {
        assert(state->traceFile == NULL);
        state->traceFile = open_memstream(&memStreamPtr, &memStreamSize);
        if (state->traceFile == NULL) {
            /* not expected */
            LOGE("Unable to open memstream\n");
            dvmAbort();
        }
    }
    assert(state->traceFile != NULL);

    fprintf(state->traceFile, "%cversion\n", TOKEN_CHAR);
    fprintf(state->traceFile, "%d\n", TRACE_VERSION);
    fprintf(state->traceFile, "data-file-overflow=%s\n",
        state->overflow ? "true" : "false");
#if defined(HAVE_POSIX_CLOCKS)
    if (!gDvm.profilerWallClock) {
        fprintf(state->traceFile, "clock=thread-cpu\n");
    } else
#endif
    {
        fprintf(state->traceFile, "clock=wall\n");
    }
    fprintf(state->traceFile, "elapsed-time-usec=%llu\n", elapsed);
    fprintf(state->traceFile, "num-method-calls=%d\n",
        (finalCurOffset - TRACE_HEADER_LEN) / TRACE_REC_SIZE);
    fprintf(state->traceFile, "clock-call-overhead-nsec=%d\n", clockNsec);
    fprintf(state->traceFile, "vm=dalvik\n");
    if ((state->flags & TRACE_ALLOC_COUNTS) != 0) {
        fprintf(state->traceFile, "alloc-count=%d\n",
            gDvm.allocProf.allocCount);
        fprintf(state->traceFile, "alloc-size=%d\n",
            gDvm.allocProf.allocSize);
        fprintf(state->traceFile, "gc-count=%d\n",
            gDvm.allocProf.gcCount);
    }
    fprintf(state->traceFile, "%cthreads\n", TOKEN_CHAR);
    dumpThreadList(state->traceFile);
    fprintf(state->traceFile, "%cmethods\n", TOKEN_CHAR);
    dumpMethodList(state->traceFile);
    fprintf(state->traceFile, "%cend\n", TOKEN_CHAR);

    if (state->directToDdms) {
        /*
         * Data is in two places: memStreamPtr and state->buf.  Send
         * the whole thing to DDMS, wrapped in an MPSE packet.
         */
        fflush(state->traceFile);

        struct iovec iov[2];
        iov[0].iov_base = memStreamPtr;
        iov[0].iov_len = memStreamSize;
        iov[1].iov_base = state->buf;
        iov[1].iov_len = finalCurOffset;
        dvmDbgDdmSendChunkV(CHUNK_TYPE("MPSE"), iov, 2);
    } else {
        /* append the profiling data */
        if (fwrite(state->buf, finalCurOffset, 1, state->traceFile) != 1) {
            int err = errno;
            LOGE("trace fwrite(%d) failed: %s\n",
                finalCurOffset, strerror(err));
            dvmThrowExceptionFmt("Ljava/lang/RuntimeException;",
                "Trace data write failed: %s", strerror(err));
        }
    }

    /* done! */
    free(state->buf);
    state->buf = NULL;
    fclose(state->traceFile);
    state->traceFile = NULL;

    /* wake any threads that were waiting for profiling to complete */
    dvmBroadcastCond(&state->threadExitCond);
    dvmUnlockMutex(&state->startStopLock);
}


/*
 * Open a dump.<TID> file for dumping the method trace into.
 */
bool prep_log(Thread *self) {
    if (!gDvm.methodTrace.traceEnabled) return false;

    char filename[64];
    if (gDvm.tracepath == 0) sprintf(filename,"%s.%d.%d","/sdcard/dump"    ,getpid(),self->systemTid);
    else                     sprintf(filename,"%s.%d.%d","/data/trace/dump",getpid(),self->systemTid);
    self->dump = fopen(filename,"a");
    if (self->dump == NULL) {
        int err = errno;
        LOGD("Could not open method trace output for %s: %s\n", filename, strerror(err));
        return false;
    }
    LOGD("Method trace output file %s is open @ %p\n",filename,self->dump);
    return true;
}


/*
 * Convert a class descriptor into its usual format. Caller must free the result.
 */
char *convertDescriptor(const char *descriptor) {

    int len = strlen(descriptor);

    if (len == 1) {
        char *class_descriptor = (char *) malloc(sizeof(char) * 8);
        if (class_descriptor == NULL) return NULL;

        memset(class_descriptor, 0, 8);

        switch(*descriptor) {
            case 'V': { sprintf(class_descriptor, "void"   ); break; }
            case 'Z': { sprintf(class_descriptor, "boolean"); break; }
            case 'B': { sprintf(class_descriptor, "byte"   ); break; }
            case 'S': { sprintf(class_descriptor, "short"  ); break; }
            case 'I': { sprintf(class_descriptor, "int"    ); break; }
            case 'F': { sprintf(class_descriptor, "float"  ); break; }
            case 'J': { sprintf(class_descriptor, "long"   ); break; }
            case 'D': { sprintf(class_descriptor, "double" ); break; }
            case 'C': { sprintf(class_descriptor, "char"   ); break; }
        }
        return class_descriptor;
    }

    if (*descriptor == '[') {
        /* recursion! */
        char *rest = convertDescriptor(descriptor + 1);

        /* allocate enough space for <rest> plus 3: ....[]\0 */
        len = strlen(rest) + 3;
        char *class_descriptor = (char *) malloc(sizeof(char) * len);
        if (class_descriptor == NULL) return NULL;

        memset(class_descriptor, 0, len);
        sprintf(class_descriptor, "%s[]", rest);
        free(rest);

        return class_descriptor;
    }

    char *class_descriptor = (char *) malloc(sizeof(char) * len);

    const char *src;
    char *dst;
    for (dst = class_descriptor, src = descriptor + 1; *src != 0; src++, dst++) {
        if (*src == '/') *dst = '.';
        else             *dst = *src;
    }
    class_descriptor[len-2] = 0;

    return class_descriptor;
}


/*
 * Given an object, call its .toString() function and return this as a C
 * string. Caller must free the result.
 */
char *objectToString(Thread *self, Object *object) {
    char *str = (char *) malloc(128 * sizeof(char));
    if (str == NULL) return NULL;

    if (!gDvm.tostring) {
        sprintf(str,"%p",object);
        return str;
    }

    if (object == 0)               return strcpy(str,"null\0");
    if (!dvmIsValidObject(object)) return strcpy(str,"invalid\0");
    if (dvmCheckException(self))   return strcpy(str,"except\0"); /* exception pending */

    /* Get the toString() method */
    Method *toString = object->clazz->vtable[gDvm.voffJavaLangObject_toString];
    assert(dvmCompareNameDescriptorAndMethod("toString", "()Ljava/lang/String", toString) == 0);

    /* Execute the toString() method */
    JValue result;
    dvmCallMethod(self, toString, object, &result);

    /* This may result in an exception being thrown by buggy .toString() implementations, which we have to clear */
    if (dvmCheckException(self)) {
        dvmClearException(self);
        return strcpy(str,"toString failed\0"); /* toString() failed */
    }

    /* <result.l> now contains a StringObject containing the toString() value of <object>. Convert it to a C string. */
    if (result.l == 0) return strcpy(str,"l==0\0");
    char *string = dvmCreateCstrFromString(result.l);

    /* convert newlines and quotes */
    char *p;
    for (p = string; *p; p++) {
        if (*p == '\n' || *p == '\r') *p = ' ';
        if (*p == '"')                *p = '\'';
    }

    /* Free our temporary result-string, as we should have a proper one by now */
    free(str);

    return string;
}

/*
 * Given a method, get its modifiers. Caller must free the result.
 */
char *getModifiers(const Method* method, u4 *args) {
    char *modifiers = (char *) malloc(128 * sizeof(char));
    if (modifiers == NULL) return NULL;

    memset(modifiers, 0, 128);

    if (dvmIsAbstractMethod    (method)) strcat(modifiers, "abstract "    );
    if (dvmIsFinalMethod       (method)) strcat(modifiers, "final "       );
    if (dvmIsNativeMethod      (method)) strcat(modifiers, "native "      );
    if (dvmIsPrivateMethod     (method)) strcat(modifiers, "private "     );
    if (dvmIsProtectedMethod   (method)) strcat(modifiers, "protected "   );
    if (dvmIsPublicMethod      (method)) strcat(modifiers, "public "      );
    if (dvmIsStaticMethod      (method)) strcat(modifiers, "static "      );
    if (dvmIsSynchronizedMethod(method)) strcat(modifiers, "synchronized ");
    if (args != NULL) strcat(modifiers, "inline ");

    return modifiers;
}

/*
 * Given a parameter (low/high) and its descriptor, get its string
 * representation ( "(<type>) <value>" ). Caller must free the result.
 */
char *parameterToString(Thread *self, const char *descriptor, u4 low, u4 high) {
    char *result = (char *) malloc(sizeof(char) * 128);
    if (result == NULL) return NULL;

    memset(result, 0, 128);

    switch (descriptor[0]) {
        case 'Z': { if ((bool)low) sprintf(result, "(boolean) \"true\"");
                    else           sprintf(result, "(boolean) \"false\"");  break; }

        case 'B': { sprintf(result, "(byte) \"%hhd\"", (s1) low);    break; }
        case 'S': { sprintf(result, "(short) \"%hd\"", (s2) low);    break; }
        case 'I': { sprintf(result, "(int) \"%d\"",    (s4) low);    break; }
        case 'F': { sprintf(result, "(float) \"%f\"",  (float) low); break; }

        case 'J': { 
                    long long int l;
                    memcpy( ((char *)&l)+0, &low,  sizeof(low)  );
                    memcpy( ((char *)&l)+4, &high, sizeof(high) );

                    sprintf(result, "(long) \"%lld\"", l); 
                    break; 
                  }
        case 'D': { 
                    double d;
                    /* By converting the double to a char pointer, we can jump
                     * to an offset within the double. We can then easily
                     * memcpy the low and high values.
                     */
                    memcpy( ((char *)&d)+0, &low,  sizeof(low)  );
                    memcpy( ((char *)&d)+4, &high, sizeof(high) );

                    sprintf(result, "(double) \"%f\"", d); 
                    break; 
                  }

        case 'V': { sprintf(result, "(void)"); break; }

        case 'C': {
                    /* dvmConvertUtf16ToUtf8() expects a string */
                    u2 str = (u2) low;

                    /* allocate enough space */
                    char *c = malloc(dvmUtf16_utf8ByteLen(&str, 1));

                    /* convert the utf16 string of size 1 to a utf8 string */
                    dvmConvertUtf16ToUtf8(c, &str, 1);

                    /* convert newlines and quotes */
                    char *p;
                    for (p = c; *p; p++) {
                        if (*p == '\n' || *p == '\r') *p = ' ';
                        if (*p == '"')                *p = '\'';
                    }

                    /* setup the parameter string */
                    sprintf(result, "(char) \"%s\"", c);

                    /* free the utf8 string */
                    free(c);

                    break;
                  }

        case '[': /* fall through */
        case 'L': {
                    /* free the temporary result string, as we may need a larger buffer */
                    free(result);

                    /* convert descriptor string to its usual format */
                    char *descriptorClass = convertDescriptor(descriptor);

                    /* get a string representation of the object */
                    char *string = objectToString(self, (Object *) low);

                    /* allocate enough memory to store this string plus some extras */
                    result = (char *) malloc(sizeof(char) * (strlen(descriptorClass) + strlen(string)) + 32);

                    /* setup the parameter string */
                    sprintf(result, "(%s) \"%s\"", descriptorClass, string);

                    /* free the descriptor string */
                    free(descriptorClass);

                    /* free the string representation, as we copied it into the result string */
                    free(string);

                    break;
                  }
    }
    return result;
}

/*
 * Return <this> of the current method, or NULL if the method is static. Caller
 * must free the result. As seen in interp/Interp.c --> dvmGetThisPtr()
 */
char *getThis(Thread *self, const Method *method, u4 *args) {

    if (dvmIsStaticMethod(method)) return NULL;
    if (args != NULL)              return objectToString(self, (Object *) args[0]);

    u4 *frameptr = self->curFrame;
    return objectToString(self, (Object* ) frameptr[method->registersSize - method->insSize]);
}

/*
 * Populate an string array of parameters by looping over the registers of the
 * current frame. Caller must free the result.
 */
char **getParameters(Thread *self, const Method *method, int parameterCount, u4 *args) {

    /* string array that will contain the parameters */
    char **parameters = malloc(parameterCount * sizeof(char *));
    if (parameters == NULL) return NULL;

    DexParameterIterator dpi;
    dexParameterIteratorInit(&dpi, &method->prototype);

    /* frame pointer */
    const u4 *frameptr;

    /* number of locals for this method */
    int locals;

    /* populate frameptr and locals */
    if (args == NULL) {
        frameptr = self->curFrame;
        locals   = method->registersSize - method->insSize;
    } else {
        /* arguments will be in args[0], args[1], args[2] and args[3] */
        frameptr = args;
        locals   = 0;
    }

    /* loop over registers and get a string representation of them. Skip the first register if this is a <this> reference */
    int i, j;
    for (i = locals + (dvmIsStaticMethod(method) ? 0 : 1), j = 0; parameterCount > 0; parameterCount--, i++, j++) {
        const char *descriptor = dexParameterIteratorNextDescriptor(&dpi);
        parameters[j] = parameterToString(self, descriptor, frameptr[i], frameptr[i + 1]);

        /* 64 bit fields (longs and doubles) use two registers. make sure we skip the next register */
        if (descriptor[0] == 'J' || descriptor[0] == 'D')
            i++;
    }

    return parameters;
}

/*
 * Combine the parameters from a provided string array into one printable
 * string. Caller must free the result.
 */
char *getParameterString(Thread *self, const Method *method, char **parameters, int parameterCount) {

    int i;

    /* concatenate the parameters */
    int len = 0;
    for (i = 0; i < parameterCount; i++) len += strlen(parameters[i]);
    char *parameterString = (char *) malloc((sizeof(char) * len) + (parameterCount * 8) + 1);
    if (parameterString == NULL) return NULL;
    memset(parameterString, 0, sizeof(parameterString));

    for (i = 0; i < parameterCount; i++) {
        strcat(parameterString, parameters[i]);

        /* append ", " only if another parameter follows */
        if (i + 1 < parameterCount) {
            strcat(parameterString, ", ");
        }
    }

    return parameterString;
}

/*
 * Generate whitespace of <depth> characters long including timestamp (if necessary). Caller must free the result.
 */
char *getWhitespace(int depth) {
    /* gDvm.timestamp will be either 0 or the number of characters "%llu: " will occupy */
    char *whitespace = (char *) malloc((depth * sizeof(char)) + gDvm.timestamp + 1);
    if (whitespace == NULL) return NULL;

    if (gDvm.timestamp) sprintf(whitespace, "%llu: ", getTimeInUsec());

    /* Fill the whitespace */
    memset(whitespace + gDvm.timestamp, ' ', depth);

    /* Append \0 */
    whitespace[gDvm.timestamp + depth] = 0;

    return whitespace;
}

void handle_method(Thread *self, const Method *method, MethodTraceState *state, int type, u4 *args) {
    int i;
    bool isConstructor = false;
    if (dvmIsConstructorMethod(method)) isConstructor = true;

    /* number of arguments for this method */
    int parameterCount = dexProtoGetParameterCount(&method->prototype);
    if (!gDvm.parameters) parameterCount = 0;

    char *whitespace         = getWhitespace(self->depth);
    char *modifiers          = getModifiers(method, args);
    char *return_type        = convertDescriptor(dexProtoGetReturnType(&method->prototype));
    char *classDescriptor    = convertDescriptor(method->clazz->descriptor);
    char *this               = NULL;
    if (!isConstructor) this = getThis(self, method, args);
    char **parameters        = getParameters(self, method, parameterCount, args);
    char *parameterString    = getParameterString(self, method, parameters, parameterCount);

#if LOGD_TRACE_ENABLED
    if (isConstructor){     LOGD_TRACE("%snew %s(%s)\n",             whitespace,                         classDescriptor,                     parameterString); }
    else {
        if (this == NULL) { LOGD_TRACE("%s%s%s %s.%s(%s)\n",         whitespace, modifiers, return_type, classDescriptor,       method->name, parameterString); }
        else {              LOGD_TRACE("%s%s%s %s(\"%s\").%s(%s)\n", whitespace, modifiers, return_type, classDescriptor, this, method->name, parameterString); }
    }
#endif

    free(whitespace);
    free(modifiers);
    free(return_type);
    if (this != NULL) free(this);
    for (i = 0; i < parameterCount; i++)  free(parameters[i]);
    free(parameters);
    free(parameterString);
    free(classDescriptor);

    if (args != NULL) free(args);

}

void handle_return(Thread *self, const Method *method, MethodTraceState *state, JValue *retval) {
    char *whitespace = getWhitespace(self->depth);

    /* parameterToString() expects two u4 parameters. We will have to split retval in half. */
    u4 low  = (retval == 0) ? 0 : (u4)  *((u8*)retval);
    u4 high = (retval == 0) ? 0 : (u4) (*((u8*)retval) >> 32);
    char *returnString = parameterToString(self, dexProtoGetReturnType(&method->prototype), low, high);

#if LOGD_TRACE_ENABLED
    LOGD_TRACE("%sreturn %s\n", whitespace, returnString);
#endif

    free(whitespace);
    free(returnString);
}

void handle_throws(Thread *self, const Method *method, MethodTraceState *state, JValue *retval, int action) {
    char *whitespace = getWhitespace(self->depth);
    char *classDescriptor;
    /* when action == METHOD_TRACE_UNROLL, retval will be an Object* exception */
    if (action == METHOD_TRACE_EXIT) classDescriptor = convertDescriptor(dvmGetException(self)->clazz->descriptor);
    else                             classDescriptor = convertDescriptor(  ((Object*) retval )->clazz->descriptor);

#if LOGD_TRACE_ENABLED
    LOGD_TRACE("%sthrows %s\n", whitespace, classDescriptor);
#endif

    free(whitespace);
    free(classDescriptor);
}
/*
 * We just did something with a method.  Emit a record.
 *
 * Multiple threads may be banging on this all at once. We use atomic ops
 * rather than mutexes for speed.
 */
void dvmMethodTraceAdd(Thread* self, const Method* method, int action, int type, void* options) {
    MethodTraceState* state = &gDvm.methodTrace;
#if DMTRACE_ENABLED
    u4 clockDiff, methodVal;
    int oldOffset, newOffset;
    u1* ptr;
#else

    if (method == NULL) {
        /* Not sure what's going on, but we better not try. */
        return;
    }

    /* If the bytecode of the caller of this method is from a system jar, then
     * enter only if the current method code is not from a system jar.
     *
     * If you would like to see *everything*, you can remove this. You would
     * then probably have to increase the launch timeout to ensure packages can
     * still be started while logging with adb though.
     *
     * This also removes trace output from system threads (GC, Binder, HeapWorker, ...).
     */

    ClassObject *caller_clazz ;

    if (type == TRACE_INLINE) {
        /* For inline functions, we must compare against the class descriptor found in the SAVEAREA. */
        const StackSaveArea* saveArea = SAVEAREA_FROM_FP(self->curFrame);
        caller_clazz = saveArea->method->clazz;
    } else {
        /* For all other invocations, we lookup the caller class of the method */
        caller_clazz = dvmGetCallerClass(self->curFrame);
    }

    if ( caller_clazz != NULL &&
         caller_clazz->pDvmDex != NULL   &&
         caller_clazz->pDvmDex->isSystem &&
        method->clazz->pDvmDex != NULL   &&
        method->clazz->pDvmDex->isSystem) {
        /* We are currently executing bytecode from a system jar, while the
         * bytecode that called the current method is also from a system jar.
         * This is not interesting for us.
         */
/*      LOGD_TRACE("pDvmDex->filename: %s\n",caller_clazz->pDvmDex->fileName); */

        const Method *caller_method = dvmGetCallerMethod(self->curFrame);

        /* java.lang.reflect.Method.invoke() is an exception, we'll trace these
         * calls always.
         *
         * We probably want to trace the creation of new class instances
         * as well (java.lang.reflect.Constructor.newInstance()), and maybe
         * even more. Have to figure out how to do this exactly (TODO).
         */
        if ( !(strcmp(caller_clazz->descriptor,"Ljava/lang/reflect/Method;") == 0 &&
               strcmp(caller_method->name,"invoke") == 0) ) {
           return;
        }
    }

    /* Hide calls to java.lang.reflect.Method.invoke() and
     * java.lang.reflect.Method.invokeNative() which is called by invoke(). We
     * already print the final method invocation.
     *
     * We probably want to hide more java.lang.reflect.* code (TODO).
     */
    if (  strcmp(method->clazz->descriptor,"Ljava/lang/reflect/Method;") == 0 &&
          ( strcmp(method->name,"invoke")       == 0 ||
            strcmp(method->name,"invokeNative") == 0)) {
          return;
    }

    /* Only enter if we did not enter earlier to avoid inception. This happens
     * when we call toString() via objectToString().
     *
     * This may be removed if we do not trace system jar code anyway.
     */
    if (self->inMethodTraceAdd) {
        return;
    }

    /* We are now in dvmMethodTraceAdd() */
    self->inMethodTraceAdd = true;
#endif

#if DMTRACE_ENABLED
    /*
     * We can only access the per-thread CPU clock from within the
     * thread, so we have to initialize the base time on the first use.
     * (Looks like pthread_getcpuclockid(thread, &id) will do what we
     * want, but it doesn't appear to be defined on the device.)
     */
    if (!self->cpuClockBaseSet) {
        self->cpuClockBase = getClock();
        self->cpuClockBaseSet = true;
        //LOGI("thread base id=%d 0x%llx\n",
        //    self->threadId, self->cpuClockBase);
    }

    /*
     * Advance "curOffset" atomically.
     */
    do {
        oldOffset = state->curOffset;
        newOffset = oldOffset + TRACE_REC_SIZE;
        if (newOffset > state->bufferSize) {
            state->overflow = true;

            /* We are leaving dvmMethodTraceAdd(). */
            self->inMethodTraceAdd = false;
            return;
        }
    } while (android_atomic_release_cas(oldOffset, newOffset,
            &state->curOffset) != 0);

    //assert(METHOD_ACTION((u4) method) == 0);

    u8 now = getClock();
    clockDiff = (u4) (now - self->cpuClockBase);

    methodVal = METHOD_COMBINE((u4) method, action);
#else

    if (action == METHOD_TRACE_ENTER) {
        /* We are entering a method... */

        handle_method(self, method, state, type, (u4 *) options);
        self->depth++;

    } else if  (action == METHOD_TRACE_EXIT && !dvmCheckException(self)) {
        /* We are returning from a method... */

        self->depth = (self->depth == 0 ? 0 : self->depth-1);
        handle_return(self, method, state, (JValue *) options);

    } else if ((action == METHOD_TRACE_EXIT &&  dvmCheckException(self)) ||
               (action == METHOD_TRACE_UNROLL)) {
        /* We are unrolling... */
        self->depth = (self->depth == 0 ? 0 : self->depth-1);
        handle_throws(self, method, state, (JValue *) options, action);

    }

    /* We are leaving dvmMethodTraceAdd(). */
    self->inMethodTraceAdd = false;
#endif

#if DMTRACE_ENABLED
    /*
     * Write data into "oldOffset".
     */
    ptr = state->buf + oldOffset;
    *ptr++ = self->threadId;
    *ptr++ = (u1) methodVal;
    *ptr++ = (u1) (methodVal >> 8);
    *ptr++ = (u1) (methodVal >> 16);
    *ptr++ = (u1) (methodVal >> 24);
    *ptr++ = (u1) clockDiff;
    *ptr++ = (u1) (clockDiff >> 8);
    *ptr++ = (u1) (clockDiff >> 16);
    *ptr++ = (u1) (clockDiff >> 24);
#endif
}

/*
 * We just did something with a method.  Emit a record by setting a value
 * in a magic memory location.
 */
void dvmEmitEmulatorTrace(const Method* method, int action)
{
#ifdef UPDATE_MAGIC_PAGE
    /*
     * We store the address of the Dalvik bytecodes to the memory-mapped
     * trace page for normal Java methods.  We also trace calls to native
     * functions by storing the address of the native function to the
     * trace page.
     * Abstract methods don't have any bytecodes, so we don't trace them.
     * (Abstract methods are never called, but in Dalvik they can be
     * because we do a "late trap" to a native method to generate the
     * abstract method exception.)
     */
    if (dvmIsAbstractMethod(method))
        return;

    u4* pMagic = (u4*) gDvm.emulatorTracePage;
    u4 addr;

    if (dvmIsNativeMethod(method)) {
        /*
         * The "action" parameter is one of:
         *   0 = ENTER
         *   1 = EXIT
         *   2 = UNROLL
         * To help the trace tools reconstruct the runtime stack containing
         * a mix of Java plus native methods, we add 4 to the action if this
         * is a native method.
         */
        action += 4;

        /*
         * Get the address of the native function.
         * This isn't the right address -- how do I get it?
         * Fortunately, the trace tools can get by without the address, but
         * it would be nice to fix this.
         */
         addr = (u4) method->nativeFunc;
    } else {
        /*
         * The dexlist output shows the &DexCode.insns offset value, which
         * is offset from the start of the base DEX header. Method.insns
         * is the absolute address, effectively offset from the start of
         * the optimized DEX header. We either need to return the
         * optimized DEX base file address offset by the right amount, or
         * take the "real" address and subtract off the size of the
         * optimized DEX header.
         *
         * Would be nice to factor this out at dexlist time, but we can't count
         * on having access to the correct optimized DEX file.
         */
        assert(method->insns != NULL);
        const DexOptHeader* pOptHdr = method->clazz->pDvmDex->pDexFile->pOptHeader;
        addr = (u4) method->insns - pOptHdr->dexOffset;
    }

    *(pMagic+action) = addr;
    LOGVV("Set %p = 0x%08x (%s.%s)\n",
        pMagic+action, addr, method->clazz->descriptor, method->name);
#endif
}

/*
 * The GC calls this when it's about to start.  We add a marker to the
 * trace output so the tool can exclude the GC cost from the results.
 */
void dvmMethodTraceGCBegin(void)
{
    TRACE_METHOD_ENTER(dvmThreadSelf(), gDvm.methodTrace.gcMethod, TRACE_PROFILE, NULL);
}
void dvmMethodTraceGCEnd(void)
{
    TRACE_METHOD_EXIT(dvmThreadSelf(), gDvm.methodTrace.gcMethod, TRACE_PROFILE, NULL);
}

/*
 * The class loader calls this when it's loading or initializing a class.
 */
void dvmMethodTraceClassPrepBegin(void)
{
    TRACE_METHOD_ENTER(dvmThreadSelf(), gDvm.methodTrace.classPrepMethod, TRACE_PROFILE, NULL);
}
void dvmMethodTraceClassPrepEnd(void)
{
    TRACE_METHOD_EXIT(dvmThreadSelf(), gDvm.methodTrace.classPrepMethod, TRACE_PROFILE, NULL);
}


/*
 * Enable emulator trace info.
 */
void dvmEmulatorTraceStart(void)
{
    /* If we could not map the emulator trace page, then do not enable tracing */
    if (gDvm.emulatorTracePage == NULL)
        return;

    updateActiveProfilers(1);

    /* in theory we should make this an atomic inc; in practice not important */
    gDvm.emulatorTraceEnableCount++;
    if (gDvm.emulatorTraceEnableCount == 1)
        LOGD("--- emulator method traces enabled\n");
}

/*
 * Disable emulator trace info.
 */
void dvmEmulatorTraceStop(void)
{
    if (gDvm.emulatorTraceEnableCount == 0) {
        LOGE("ERROR: emulator tracing not enabled\n");
        return;
    }
    updateActiveProfilers(-1);
    /* in theory we should make this an atomic inc; in practice not important */
    gDvm.emulatorTraceEnableCount--;
    if (gDvm.emulatorTraceEnableCount == 0)
        LOGD("--- emulator method traces disabled\n");
}


/*
 * Start instruction counting.
 */
void dvmStartInstructionCounting()
{
    updateActiveProfilers(1);
    /* in theory we should make this an atomic inc; in practice not important */
    gDvm.instructionCountEnableCount++;
}

/*
 * Start instruction counting.
 */
void dvmStopInstructionCounting()
{
    if (gDvm.instructionCountEnableCount == 0) {
        LOGE("ERROR: instruction counting not enabled\n");
        dvmAbort();
    }
    updateActiveProfilers(-1);
    gDvm.instructionCountEnableCount--;
}


/*
 * Start alloc counting.  Note this doesn't affect the "active profilers"
 * count, since the interpreter loop is not involved.
 */
void dvmStartAllocCounting(void)
{
    gDvm.allocProf.enabled = true;
}

/*
 * Stop alloc counting.
 */
void dvmStopAllocCounting(void)
{
    gDvm.allocProf.enabled = false;
}
