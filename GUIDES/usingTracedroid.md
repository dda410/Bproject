[comment]: <> (auto-fill-mode, flyspell-mode, markdown-preview-mode)

# USING TRACEDROID

First androguard should be installed as shown in the previous
guide. 

Second create the avd and run the emulator as shown in the
creatingAVDforTracedroidGuide.md.

Now let's start the app we want to store the traces of:

`adb shell am start -a android.intent.action.VIEW -n
com.android.browser/.BrowserActivity -d about:blank`

In this case is the android stock browser. After this let's get the
uid of the started app:

`adb shell dumpsys package com.android.browser | grep userId=`

Every app started by android it is started with its own user id. Now
that we know the uid we can kill the app process with:

`adb shell ps | grep com.android.browser | awk '{print $2}' | xargs
adb shell kill`

After killing the process we create a new file called "uid" containing
the uid of the app we want to trace. In this case our "uid" file will
contain just one line writing "10022". We need to push this file to
the sdcard/ directory of the android avd we are using in order to
store traces next time it will be launched:

`adb push uid /sdcard/`

We can now restart the app with:

`adb shell am start -a android.intent.action.VIEW -n
com.android.browser/.BrowserActivity -d about:blank`

The traces will be stored under the /sdcard/ directory of the emulated
avd.

In order to pull the traces to a local directory run:

`adb pull "sdcard/"`