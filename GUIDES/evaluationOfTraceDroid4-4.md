# TRACEDROID 4.4 EVALUATION

We present in this guide how the evaluation of TraceDroid 4.4 was
performed. To evaluate the system we tested both a stock application
and an external one. The tests were performed by emulating the custom
TraceDroid 4.4 image (as explained in the previous guides) and
interacting with it via the adb CLI.

#### TESTING STOCK APPLICATION

We decided to test the default Android browser. Other default
applications can be tested with in the same way. Just substitute
com.android.browser with the default app's package name to be tested
and select and app activity to start. We did the following:

- Grep the uid of the app: `adb shell dumpsys package
  com.android.browser | grep userId=`
- Store it under the common input interface of the program: `echo
  "10018 > uid; adb shell uid /sdcard/"`
- Check that the app was not already running, otherwise
  kill it with: `adb shell ps | grep com.android.browser | awk '{print $2}' | xargs adb shell kill`
- Start one of the activities of the browser (namely opening
  an empty tab) like shown in previous guides: `adb shell am start -a android.intent.action.VIEW -n com.android.browser/.BrowserActivity -d about:blank`
- After the process terminated pull the dump files store via the
  common output interface with: `adb pull /sdcard/`
- The dump files are now ready to be analyzed

#### TESTING EXTERNAL APPLICATION

The external applications we tested
are [aptoide](https://en.aptoide.com/), a famous alternative app store, [firefox](https://www.mozilla.org/en-US/firefox/android/),
Mozilla's Android mobile browser and [waze](https://www.waze.com/), a
widely used mobile navigation app.  Any other application with api
level <= 19 can be tested and used by TraceDroid 4.4. We first
downloaded its .apk file and then performed the following:

- Extract the manifest.xml file to check that minSdkVersion was
  greater than 10 (Android 2.3.4 api level) and smaller or equal to 19
  (Android 4.4 api level). This was important in order to check whether the
  compatibility of TraceDroid was increased. For this purpose we used
  ANDROGUARD framework: `androaxml.py -i file.apk -o
  file_manifest.xml`
- Install the application: `adb install file.apk`
- Get list of installed packages and filter them in order to have the
  installed app's package name: `PKG=$(adb shell 'pm list packages -f' | grep appName | awk -F"=" '{print $2}' | sed 's/.$//')`
- Grep the uid of the app: `APP_UID=$(adb shell dumpsys package $PKG | grep userId | awk -F"=" '{print $2}' | awk '{print $1}')`
- Store it under the common input interface of the program: `echo "$APP_UID" > uid && adb push uid sdcard/`
- Check that the app was not already running, otherwise
  kill it with: `adb shell ps | grep $PKG | awk '{print $2}' | xargs adb shell kill`
- Start the application by simulating a 'tap' on it: `adb shell monkey
  -p $PKG -c android.intent.category.LAUNCHER 1`
- Optionally other activities can be launched by looking at what the
  ones the app offers. To have a list of the app's activities: `adb
  shell dumpsys package $PKG`
- After the process terminated pull the dump files store via the
  common output interface with: `adb pull /sdcard/`
- The dump files are now ready to be analyzed
