# COMPILING ANDROID 4.4

In order to compile android 4.4 without having to download again the
android source code, that has already been downloaded to compile the
version 2.3, do the following:

- Log into the docker image that has been used so far: `sudo docker
  run -it ubuntu_pisellone /bin/bash`
- create a new directory under the root home: `mkdir android4.4`
- cd into the directory and then synchronize the repo by referencing
  the folder where the android source code has already been
  downloaded: `repo init -u
  https://android.googlesource.com/platform/manifest -b android-4.4_r1
  --reference=/root/ANDROID`
- run `repo sync` in order to synchronize the repo with the new
  android version, in this case android 4.4. This will download the
  additional missing components needed for this android version.
- Commit the docker image after the sync process terminates 
- In order to compile android 4.4 there must be installed java 6 from
  oracle: 
	  - Download the jdk-6u45-linux-x64.bin installer from oracle
        website
	  - Push the file to the running container with `sudo docker cp
        jdk-6u45-linux-x64.bin containerName:/usr/lib/jvm/`
	  - In the docker container command line cd to the directory: `cd
        /usr/lib/jvm/`
	  - Make the file executable: `chmod 755 jdk-6u45-linux-x64.bin`. 
	  - Run the installer `./jdk-6u45-linux-x64.bin`. This will create
        a folder in the same directory called jdk-6u45-linux-x64
	  - Now the JAVA_HOME and PATH envoironment variables need to
        updated: 
		
		```
		JAVA_HOME=jdk-6u45-linux-x64
		export JAVA_HOME
		PATH=$JAVA_HOME/bin:$PATH
		export PATH
		```
		The previous lines can be added to .bashrc profile in order to
		use the just installed java also for the future sessions.
- Go back to the previously created folder android4.4 and run
  `. build/envsetup.sh` and after run `make`. This will compile the
  system.img for android 4.4.
