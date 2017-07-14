# COMPILE TRACEDROID ON ANDROID 4.4

After porting the tracedroid changes from android 2.3 to android 4.4
(see the github commits to see how the code has been ported) the code
needs to be compiled in order to output the system.img image that can
later be used to boot use tracedroid via the emulator.

## COMPILE THE dalvik/vm folder

The tracedroid code works by modifying the vm folder of the dalvik
virtual machine (dalvik folder) in the android source code. First the
TraceDroid 4.4 sources need to be downloaded. They can be found on
the [src](https://github.com/dda410/Bproject/tree/master/src) folder
of this repo. The vm/ folder of the src directory has to replace the
dalvik/vm/ folder of the Android source tree. After applying the
changes the source files need to be recompiled. In order to do this
first we need to change our current directory to the root of the one
containing the android 4.4 source code. We then type:

- `. build/envsetup.sh` to get the environment variables
- `cd dalvik/vm/` to go in the vm directory
- `mm` to compile all the files in the current directory and
  sub-directories. This will output a BLOB of the new files
  
## RECOMPILE THE WHOLE ANDROID SOURCE

Now we need to recompile the whole android source in order to link the
prevously compiled BLOB (containing the tracedroid changes) to the
android system.img. Go to the root directory of the android source
code and type:

`make`

This won't take too much time since we already compile all the code to
obtain a default system.img. The compiler now needs just to link the
new vm BLOB to the system.img one.

## PULL THE COMPILED IMAGE

After obtaining a new system.img pull this from the docker container
to the client in order to be used later by and android emulator:

`sudo docker cp docker_container_name:/root/android44/out/target/product/generic/system.img ./`
