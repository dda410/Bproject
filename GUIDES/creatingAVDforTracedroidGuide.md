[comment]: <> (auto-fill-mode, flyspell-mode, markdown-preview-mode) 

# RUNNING TRACEDROID

## ANDROID STUDIO INSTALLATION

Android studio is needed in order to run tracedroid. Before installing
tracedroid the following packages must be installed:

`sudo apt-get install libc6:i386 libncurses5:i386 libstdc++6:i386
lib32z1 libbz2-1.0:i386`

If you do not want to install the change the package architecture you
can use the 32 bit libraries available like the following:

`sudo apt-get install lib32z1 lib32ncurses5 lib32bz2-1.0 lib32stdc++6`

Look for all the lib32 packages corresponding to the one in the first
command issued. if some packages are missing look into the debian
wheezy backports.

Download then the android studio package
from [https://developer.android.com](https://developer.android.com) and unzip it in /opt 
. After this navigate to android-studio/bin/ and execute the
./studio.sh script with sudo privileges: `sudo ./studio.sh`

Follow the installation steps in order to complete the installation of
android studio, downloading all the required resources.

## UPDATE JAVA TO JAVA 8 

Since android studio compiles with java 8 also the java runtime in the
system must be 8 or higher. In order to have a working java runtime
the latest jdk from oracle must be downloaded and installed. In order
to install java 8 from oracle repos do the following:

- add the java ppa repos: `sudo vim
  /etc/apt/sources.list.d/java-8-debian.list`
  and then add the following lines: 
  `deb http://ppa.launchpad.net/webupd8team/java/ubuntu trusty main`
  `deb-src http://ppa.launchpad.net/webupd8team/java/ubuntu trusty main`

- install java in the system: 
  `sudo apt-get update`
  `sudo apt-get install oracle-java8-installer`
  
- Configure the java environment:
  `sudo apt-get install oracle-java8-set-default`
  
The source for the java installation is taken from
[(https://tecadmin.net/install-java-8-on-debian/]
(https://tecadmin.net/install-java-8-on-debian/).

## CREATE AND RUN AVD (ANDROID VIRTUAL DEVICE)

Install the android-10 sdk:
`./Android/Sdk/tools/bin/sdkmanager "platforms;android-10"`

Modify the hardware.ini file located in
<android-sdk>/platforms/android-10/skins/WVGA800/hardware.ini with the
following lines: 

```
vm.heapSize=64
hw.ramSize=1024
```

pull the compiled tracedroid .img from the docker container to the
current directory with:

`sudo docker cp happy_morse:/root/ANDROID/out/target/product/generic/system.img ./`

Set variables to create and run avd:

```
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SYSTEM_TARGET=system.img 
AVD=android.2.3.3
```

Create a new avd:

`avdmanager create avd -n $AVD -k "platforms;android-10" --abi armeabi --sdcard 256M --force`

Run the avd with the emulator:

```
emulator -avd $AVD -system $SYSTEM_TARGET -no-window &
pid=$!
output=''; while [[ "$output" != *package* ]]; do sleep 5; output=$(adb shell pm path android); done
```

Start the browser to be sure everything is working:

`adb shell am start -a android.intent.action.VIEW -n com.android.browser/.BrowserActivity -d about:blank`

Kill the emulator:

`kill $pid`

Move the AVD to a .backup so it can be used by emudroid:

```
rm ~/.android/avd/$AVD.avd.backup -Rf;
rm ~/.android/avd/$AVD.ini.backup -Rf;
mv ~/.android/avd/$AVD.avd ~/.android/avd/$AVD.avd.backup;
mv ~/.android/avd/$AVD.ini ~/.android/avd/$AVD.ini.backup;
```

The emaulator and avdmanager command must be linked in system bins in
order to be reachable, otherwise they must be found in the
<android-sdk> subfolders. 

This guide is taken from
[https://github.com/vvdveen/tap-oss/blob/master/tools/build-avd.sh](https://github.com/vvdveen/tap-oss/blob/master/tools/build-avd.sh)
