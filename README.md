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

The repo has different folders and files, the structure is the
following:

- Android_2.3: it contains a working system.img of android 2.3
  compiled with the Tracedroid framework. it contains the uid file
  that indicates the uid of the file we want to trace and an sdcard
  folder containing the traces stored for the application with that
  uid. 
- JavaOracle6Installer: contains the oracle binary file to install
  java 6 (required to compile android 4.4).
- OTHERS: contains some files used during installation, compilation
  and debugging that can be relevant in the future.
- PDFs: contains papers and guides relevant to the subject of dynamic
  malware analysis. 
- GUIDES: contains the guides and howtos that explain the steps that
  need to be taken in order to compile, install and run Tracedroid and
  also the steps required to port Tracedroid to android 4.4:
	  
  - [compilingTracedroidGuide](https://github.com/dda410/Bproject/blob/master/GUIDES/compilingTracedroidGuide.md):
	  it explains how to compile the Tracedroid android 2.3
	  system.img within an ubuntu 12.04 docker container. All the
	  required libraries and programs used in the process are
	  explained step by step.
  - [creatingAVDforTracedroidGuide](https://github.com/dda410/Bproject/blob/master/GUIDES/creatingAVDforTracedroidGuide.md):
	  it explains how to setup and install the android sdk tools that
	  are needed to emulate and run the compiled system.img. Further
	  more it is explained how to create and AVD (Android Virtual
	  Device) in order to make the system.img runnable by the
	  emulator.
  - [installingAndroguard](https://github.com/dda410/Bproject/blob/master/GUIDES/installingAndroguard.md):
	  This guide is optional and it explains how to install androguard
	  in order to provide also static malware analysis of the android
	  applications. 
  - [usingTracedroid](https://github.com/dda410/Bproject/blob/master/GUIDES/usingTracedroid.md):
	  it explains how to use Tracedroid by using the emulator with the
	  system.img compiled image. Step by step it is explained how to
	  run an application, get is uid and start storing the traces for
	  that application.
  - [compilingAndroid4-4](https://github.com/dda410/Bproject/blob/master/GUIDES/compilingAndroid4-4.md)
	  it explains how to compile android 4.4 default system.img within
	  the previously used docker container.
