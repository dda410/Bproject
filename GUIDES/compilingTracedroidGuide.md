[comment]: <> (auto-fill-mode, flyspell-mode, markdown-preview-mode) 

# COMPILING TRACEDROID

Tracedroid is based on android 2.3 (codename gingerbread). Google
suggests as ubuntu 12.04 64 bits as the OS to be used as a building
environment. In order to avoid dependency and retro compatibility
problems I decided to use a docker with ubuntu 12.04 image as a
building environment. Following there are the steps to have a working
building environment.

## DOCKER

Docker allows linux user to use other linux based os in chroot
jails. Working with chroot jails allows to have a separate file system
that is isolated from the main operating system (adding an extra layer
of security). Docker, that is based on LXC, has furthermore the
advantage of being almost as fast as the host system since the images
of the OS run through docker uses the host linux kernel as exokernel.
This are the steps to set up docker:

- install docker for your distro (check online howtos)
- run ubuntu 12.04 image: `sudo docker run -it ubuntu:12.04 /bin/bash`
  this will download from docker repo a 64 bits ubuntu 12.04 minimal
  image and start a bash session.
- On another bash session run `sudo docker ps` to have a list of the
  running images.
- Do some changes to the filesystem and then commit them to have your
  a first commit of the image: 
  `sudo docker commit -m "First commit" [id of the image to commit]
  Tracedroid_envoironment`
  The [id of the image to commit] is shown when listing the images
  that are running (hence the containers) with `sudo docker
  ps`. Tracedroid_envoironment in the commit command is the new name
  we assign to our modified image.
- Now let's go back to the shell of the ubuntu 12.04 image.
  
## INSTALL DEPENDENCIES FOR ANDROID SDK COMPILATION

The following are a series of commands to prepare the building
envoironment with the right dependencies (to perform from the shell
opened by launching the ubuntu 12.04 image from docker):

- Update the system with: `apt-get update`
- Install the c and c++ compilers, both 4.4 version, the one
  compatible with android 2.3: 
  `apt-get install gcc-4.4-multilib g++-4.4-multilib`
- In the case previous versions of gcc and g++ and cpp where already
  installed do the following commands to change the
  default compilers:
  ```
  ln -s /usr/bin/g++-4.4 g++
  ln -s /usr/bin/gcc-4.4 gcc
  ln -s /usr/bin/cpp-4.4 cpp
  ```
- Install openjdk 6, the java compiler used to compile android 2.3:
  `apt-get install openjdk-6-jdk`
 
- Install the ubuntu 12.04 dependencies for building android as google
  suggests (http://source.android.com/source/initializing.html):
  ```
  apt-get install git gnupg flex bison gperf build-essential \
  zip curl libc6-dev libncurses5-dev:i386 x11proto-core-dev \
  libx11-dev:i386 libreadline6-dev:i386 libgl1-mesa-glx:i386 \
  libgl1-mesa-dev g++-multilib mingw32 tofrodos \
  python-markdown libxml2-utils xsltproc zlib1g-dev:i386
  ```
- Change libgl link as google suggests:
  `ln -s /usr/lib/i386-linux-gnu/mesa/libGL.so.1
  /usr/lib/i386-linux-gnu/libGL.so`
- Install the following package to avoid the ld command (the linker)
  outputting errors at compile time:
  `apt-get install libz-dev`
- Install repo google tool to fetch the code from the android tree
  source code:
  ```
  mkdir ~/bin
  PATH=~/bin:$PATH
  curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
  chmod a+x ~/bin/repo
  ```
  To have repo available also in future sessions export the path to
  .bashrc.
  
## REPO INITIALIZATION

Now it is time to add a working directory:

```
mkdir WORKING_DIRECTORY
cd WORKING_DIRECTORY
```

Set up git with your information:

```
git config --global user.name "Your Name"
git config --global user.email "you@example.com"
```

Then we initialize the repo for the version of android we wish to
compile (https://github.com/vvdveen/tap-oss): 

`repo init -u https://android.googlesource.com/platform/manifest -b android-2.3.4_r1`

After this we can download the android source tree:

`repo sync`

This will take a couple of hours to finish.

## BUILDING

Now it's time to compile the android source code. First we prepare the
building envoironment:

`. build/envsetup.sh`

Before compiling we have to modify a few source files not to occur in
compilation errors:

1. having the ANDROID build directory as root open frameworks/base/libs/utils/Android.mk
2. change line 60 from `LOCAL_CFLAGS += -DLIBUTILS_NATIVE=1
   $(TOOL_CFLAGS)` to `LOCAL_CFLAGS += -DLIBUTILS_NATIVE=1
   $(TOOL_CFLAGS) -fpermissive`
3. open frameworks/base/tools/obbtool/Android.mk
4. remove -Werror on line 16 and on line 37

Then we compile the sources with the make command:

`make`

This will take in between 1 and 2 hours depending on the processor of
the host.

## BUILDING TRACEDROID IMAGE

First we setup ssh public keys for the github: see
https://help.github.com/articles/connecting-to-github-with-ssh/

Then we perform the following (https://github.com/vvdveen/tap-oss) to
add the tracedroid source in our tree:

```
cd dalvik
git remote add tracedroid git@github.com:vvdveen/daap-dalvik.git
git fetch tracedroid
git checkout tracedroid/trace
cd ..

cd frameworks/base/
git remote add tracedroid git@github.com:vvdveen/daap-frameworks-base.git
git fetch tracedroid
git checkout tracedroid/trace
cd ../..
```

Now we can compile tracedroid:
`make`

## CONCLUSION

Everything should be correctly compiled by now. If you want to keep
the changes made to the ubuntu 12.04 image remember to commit before
exiting. 
