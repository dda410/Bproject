# PORTING TRACEDROID TO ANDROID 4.4
## BACHELOR PROJECT

The project is based on tracedroid. Tracedroid is a framework based on
Android 2.3.4 (codename gingerbread) that aims to dynamically trace
the calls Android applications (.apk) make to the dalvik virtual
machine (a modified jvm that runs Android applications) in order to
find malwares. The core of Tracedroid is based on modifying the source
files of the dalvik virtual machine (located in dalvik/vm/ under
Android source folder). This makes possible to profile, analyze and
store the calls that programs make to the dvm. Tracedroid works by
modifying the source code of a bootable Android system.img, after
recompiling the Android system.img the image itself needs to be
recompiled with the Tracedroid changes in order to work. When a
modified system.img is compiled it needs to be emulated with the
Android sdk and tools in order to use Tracedroid and start profiling.

The main goal of the project is to port Tracedroid from Android 2.3.4 to
Android 4.4 to make it compatible with more recent software. Android
4.4 is chosen as the target Android version of porting since it is the
last Android version that supports by default a dalvik virtual machine
(from version 5.0 and on the runtime changed to art).

The repo has different folders and files, the structure is the
following:

- __GUIDES__: contains the guides and howtos that explain the steps that
  need to be taken in order to compile, install and run Tracedroid and
  also the steps required to port Tracedroid to Android 4.4. We
  present them in the chronological order they should be followed:
	  
  - [compilingTracedroid2.3](https://github.com/dda410/Bproject/blob/master/GUIDES/compilingTracedroidGuide.md):
	  it explains how to compile the Tracedroid Android 2.3
	  system.img within an ubuntu 12.04 docker container. All the
	  required libraries and programs used in the process are
	  explained step by step.
  - [creatingAVDforTracedroid2.3](https://github.com/dda410/Bproject/blob/master/GUIDES/creatingAVDforTracedroidGuide.md):
	  it explains how to setup and install the Android sdk tools that
	  are needed to emulate and run the compiled system.img. Further
	  more it is explained how to create and AVD (Android Virtual
	  Device) in order to make the system.img runnable by the
	  emulator.
  - [installingAndroguard](https://github.com/dda410/Bproject/blob/master/GUIDES/installingAndroguard.md):
	  This guide is optional and it explains how to install androguard
	  in order to provide also static malware analysis of the Android
	  applications. 
  - [usingTracedroid](https://github.com/dda410/Bproject/blob/master/GUIDES/usingTracedroid.md):
	  it explains how to use Tracedroid by using the emulator with the
	  system.img compiled image. Step by step it is explained how to
	  run an application, get is uid and start storing the traces for
	  that application.
  - [compilingAndroid4.4](https://github.com/dda410/Bproject/blob/master/GUIDES/compilingAndroid4-4.md)
	  it explains how to compile Android 4.4 default system.img within
	  the previously used docker container.
  - [compileTracedroidOnAndroid4.4](https://github.com/dda410/Bproject/blob/master/GUIDES/compilingTracedroidOnAndroid44.md)
	  it explains how to compile tracedroid on top of Android 4.4
      source code.
  - [creatingAVDforAndroid4.4](https://github.com/dda410/Bproject/blob/master/GUIDES/creatingAVDforAndroid44.md)
	  it explains how to create an Android Virtual Device for Android 4.4 and how to
      emulate it using a custom system.img (the tracedroid one).
  - [evalutaionOfTraceDroid4.4](https://github.com/dda410/Bproject/blob/master/GUIDES/evaluationOfTraceDroid4-4.md)
	  it explains how to evaluate and test the TraceDroid 4.4
      implementation with both a default application and an external
      one.

- __JavaOracle6Installer__: contains the oracle binary file to install
  java 6 (required to compile Android 4.4).
- __THESIS__: contains the written thesis produced for the project. All
  the .tex files and images and the output pdf are in this folder.
- __TraceDroid_4.4__: contains the files relative to TraceDroid 4.4. Its
  subfolders are divided into:
  
  - compiledImages: contains the TraceDroid 4.4 compiled system
  images used by the Android emulator to trace the apps. Download
  these if you do not want to compile your own version of TraceDroid
  4.4. 
  - externalApk: contains the external .apk files and their
  manifest.xml files used for the evaluation of the system.
  - tracesEvaluation: contains the traces produced by the system
    evaluation phase.
	
  if the files are too big to be pushed to the github repo they are
  linked to external cloud storage via README.md files of the
  folders.
  
- __dalvikMachines__: contains the dalvik virtual machines source code
  (dalvik/vm folder in the Android source tree) of Android
  version 2.3.4 (Gingerbread) and 4.4 (KitKat). It also contains the source code of
  TraceDroid 2.3.4. Looking at the diff between Android 2.3.4 and
  TraceDroid 2.3.4 sources was a fundamental stage of the porting.
- __slides__: contains the slides used to present the project. 
- __src__: contins the TRACEDROID 4.4 source code. The source code consist
  in a modified version of the dalvik/vm/ Android source tree directory.
