# CREATING AVD FOR ANDROID 4.4 AND EMULATING IT

## CREATE AVD

First we need to download the images for android 4.4
(a.k.a. android-19):

`sdkmanager 'system-images;android-19;default;armeabi-v7a'`

We then move to the folder where our custom system.img is located and
set some variables for the avd:

```
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SYSTEM_TARGET=system.img 
AVD=tracedroid4.4
```

After this we need to create the avd:

`avdmanager create avd -n $AVD -k
"system-images;android-19;default;armeabi-v7a" --abi armeabi-v7a
--sdcard 256M --force`

Now we can check that the avd has been created by running:

`avdmanager list avd`

## EMULATE THE AVD

After creating the avd for android 4.4 we emulate it with our cooked
image:

```
emulator -avd $AVD -system $SYSTEM_TARGET -no-window &
pid=$!
output=''; while [[ "$output" != *package* ]]; do sleep 5; output=$(adb shell pm path android); done
```

Now that the emulator has been started you can interact with it and
with tracedroid through command line utility `adb`. When we finish
using it we kill the emulator with: 

`sudo kill $pid`


## ADDITIONAL NOTES

The two programs used in this guide `avdmanager` and `sdkmanager` are
located under the previously installed android sdk tools:
Sdk/tools/bin.

To see a list of the installed packages that the emulator can run
type:

`adb shell pm list packages -f`

To read the logs of the emulator (useful for debugging purposes) type:

`adb shell logcat`


