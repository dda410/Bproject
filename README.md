# TRACEDROID BACHELOR PROJECT

The project is based on tracedroid. Tracedroid is a framework based on
android 2.3 (codename gingerbread) that aims to dynamically trace the
calls android applications (.apk) make to the dalvik virtual machine
(a modified jvm that runs android applications) in order to find
malwares. The core of Tracedroid is based in the modification of the
profile.c (located in dalvik/vm/ under android source folder) source
file of the dalvik virtual machine. This makes possible to profile,
analyze and store the calls that programs make to the dvm. Tracedroid
works by modifying the source code of a bootable android system.img,
after recompiling the android system.img the image itself needs to be
recompiled with the Tracedroid changes in order to work. When a
modified system.img is compiled it needs to be emulated with the
android sdk and tools in order to use Tracedroid and start profiling.

The main goal of the project is to port Tracedroid from android 2.3 to
android 4.4 to make it compatible with more recent software. Android
4.4 is chosen as the target android version of porting since it is the
last android version that supports by default a dalvik virtual machine
(from version 5.0 and on the runtime changed to art).

In order to get everything working there are several steps that need
to be taken. I wrote a few guides that explains how to run, compile,
install and get tracedroid working
