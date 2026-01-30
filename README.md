# Installation Guide for Amdahl UTS system from 1981

## Background

The tape image used to build this system appears to be from a port of the
V7 Research UNIX onto an IBM 370 mainframe environment around 1981.
This version could only be installed and run under the IBM VM operating system as a guest OS.

Once loaded the installation includes a runnable UTS system with all of the source code of both the kernel and utilities, the entire system is capable of rebuilding itself.

The tape image was obtained from [Moshix's UTS GitHub](https://github.com/moshix/UTS).
It is in IBM CMS format and contains two tape files:

1. CMS tape loadable file containing installation text, install file and sample VM configuration.
2. A DDR format tape file containing the complete UTS system.

I believe this tape would be fully installable in a current IBM z/VM system, although I have not tried it (yet). It is, however, very easy to install, and run it on any system capable of supporting the [Hercules IBM 370 emulator](http://www.hercules-390.org/).

The following page contains details of how to install, configure and operate, this system on a Raspberry Pi.

## Prerequisites

1. A platform capable of running the [Hercules IBM 370 emulator](http://www.hercules-390.org/).  Native versions are available for Windows and source code is available with a [Hercules helper](https://github.com/wrljet/hercules-helper/blob/master/README.md) tool to help the build process on other environments, such as LINUX, MacOs and Arm64.
2. Around 1GB of disk space to  hold the VM370 and UTS disk volumes.
3. A 3270 terminal emulator - on LINUX either c3270 or x3270 will work, on Windows wc3270 is included in VM/370 build below.
4. It is worth reading up about how to enter the **Clear**, **Reset**, **PA1**, **PA2** and **Sys Req** keys on your 3270 emulator as these keys are critical for interacting with both VM and UTS.

## Installation Steps

Here are the high level steps to get UTS installed and running.

### Install Hercules

Either obtain a prebuilt [Windows](https://sdl-hercules-390.github.io/html/) installation or download the [Hercules helper](https://github.com/wrljet/hercules-helper/blob/master/README.md) tool for other platforms. Set your environment up so that you can run the **hercules** command from the command line.

### Install VM/370

Download the [Disk images for VM/370](https://www.vm370.org/VM/V1R1.2). Unpack the zip file into a "VM" directory - this directory  contains a fully installed and ready to run system. Use the **vm370ce.cmd** Windows or **vm370ce.sh**  Linux to boot the system.

Connect to VM370 using a 3270 emulator - by default the commands above will start emulator session(s).
Hit **Enter** to clear the VM370 splash screen and login using
cp	 **LOGON MAINT** password **CPCMS**
A few useful commands:
| Command | Notes|
|:---------------|:-----|
| DIR  |directory list for current Minidisk (A)|
| DIR \* \* B | directory list for Minidisk B|
| TYPE USER DIRECT | display contents of file USER DIRECT|
| Q RDR | show any files in your virtual reader|
| Q DASD FREE | show unallocated disk packs|

At some point you will  get a
**MORE...** message on the bottom right of the window - show the next screen  using the  **Clear** key. If you simply hit **Enter** the display will change to **HOLDING**,	move on using **Clear**.

To interrupt the output, or a long running command, enter the **PA2** key - this will drop you back to the CMS command prompt. If you accidentally enter **PA1** this will "kill" CMS and you will get **CP READ** displayed. At this point you can either **LOGOFF** or reIPL CMS by entering **IPL CMS**.

After you have explored things you can shutdown VM370 by entering **SHUTDOWN** from the MAINT login userid OR by entering **/shutdown** at the Hercules system console **===>** prompt.

### Install UTS
Download the UTS installation tape from https://github.com/moshix/UTS/blob/main/Amdahl_UTS2.aws.bz2 and unzip it into directory accessible from the VM370 installation, say **tapes/Amdahl_UTS2.aws**.

Create a new directory next to tapes, say **disks-uts/**,  enter the following command to create a couple of empty IBM 370 disk packs (initially we'll only use the first one):
```
dasdinit64 -a -bz2 disks-uts/uts.150 3330 UTSSYS
dasdinit64 -a -bz2 disks-uts/uts.151 3330 UTSUSR
```
Edit your previously downloaded VM370 configuration file **vm370ce.conf** and add the following two lines at the end of the file.
```
# UTS disks : 150 is standard UTSSYS remainder are "spares"
0150     3330    disks-uts/uts.150
0151     3330    disks-uts/uts.151
```
Restart Hercules and these  disks will be attached to the VM370 configuration.
If you login as MAINT/CPCMS you should see them listed if you do a **Q DASD FREE**.

Create a card deck, a text file containing the following CMS EXEC script that performs the installation of UTS from the tape, on your LINUX/Windows system, say **adduts_exec.cards**:
```
USERID MAINT
&CONTROL ERROR

&TYPE Loading UTS tape and disk
CP ATTACH 480 TO MAINT AS 181
CP ATTACH 150 TO MAINT
TAPE LOAD

&IF &RETCODE EQ 0 &GOTO -G1
&TYPE "Load of install tape file 1 failed"
&EXIT 4
******
-G1
&TYPE INSTALL EXEC ready, extracting
EXEC INSTALL
&IF &RETCODE EQ 0 &GOTO -G2
&TYPE "DDR EXTRACT OF TAPE FILE 2 FAILED"
&EXIT 4
******
-G2
&TYPE UTS DASD loaded
CP DET 150
CP DET 181
******

&TYPE Adding UTS user to directory
COPY USER DIRECT A SAVE DIRECT A
******
*NB: The UTS Install tape includes a sample UTS DIRECT entry
*    we don't use that as it needs some customisation.
*    MDISK 770 to allow additional volume to be added later
******
&STACK BOTTOM
&STACK INPUT
&BEGSTACK

USER UTS AMDAHL 6M 6M G
IPL 220
OPTION ECMODE REALTIMER BMX
CONSOLE 009 3215
SPOOL 00C 2540 READER A
SPOOL 00D 2540 PUNCH A
SPOOL 00E 1403 A
MDISK 150 3330 0   404 UTSSYS WR AMDAHL AMDAHL
MDISK DD0 3330 001 050 UTSSYS WR AMDAHL AMDHAL
MDISK 550 3330 051 050 UTSSYS WR AMDAHL AMDAHL
MDISK 110 3330 101 050 UTSSYS WR AMDAHL AMDAHL
MDISK 220 3330 151 060 UTSSYS WR AMDAHL AMDAHL
MDISK 330 3330 211 160 UTSSYS WR AMDAHL AMDAHL
MDISK 660 3330 371 030 UTSSYS WR AMDAHL AMDAHL
MDISK 770 3330 001 403 UTSUSR WR AMDAHL AMDAHL
&END
&STACK
&STACK SAVE
&STACK QUIT
EDIT USER DIRECT A (NODISP
DIRECT USER
******
&TYPE Updating AUTOLOGIN to add UTS
CP LINK AUTOLOG1 191 397 MR MULT
CP ATTACH 150 TO SYSTEM AS UTSSYS
ACCESS 397 Z
COPY PROFILE EXEC Z PROFILE BACKUP Z
******
&TYPE Editing PROFILE EXEC
&STACK TOP
&STACK LOCATE CP SET PRIOR
&STACK INPUT
&STACK * UTS VM 
&STACK CP ATTACH 150 TO SYSTEM AS UTSSYS	
&STACK CP AUTOLOG UTS AMDAHL AUTOUTS
&STACK
&STACK SAVE
&STACK QUIT
EDIT PROFILE EXEC Z (NODISP
******
&TYPE All done
&TYPE After VM370 RESTART connect using DIAL UTS
&EXIT 0
```
Copy this text into a file called, say **adduts_exec.cards"**.

We're now ready to install UTS into the running VM370 system. Using a 3270 terminal login as MAINT/CPCMS

Return to the Hercules window and at the **===>** prompt enter the following three lines one at a time:
```
/START ALL
devinit 00C adduts_exec.cards ascii eof trunc
devinit 480 tapes/Amdahl_UTS2.aws" 
```
This will (i) start the virtual card reader (ii) read in your deck (iii) mount the tape ready for use by the installation script on the cards.

Back to the 3270 MAINT window read in the card deck and if successful run the scipt by entering:
```
READCARD ADDUTS EXEC
ADDUTS
```
You should then see a series of messages as the tape is read onto the UTS disk and UTS is created as a new Virtual Machine that will automatically be started the next time you restart VM370.

Now shutdown VM370 and restart it. On the Hercules Window you should see a message like:
```
09:56:57 DASD 150 ATTACH TO SYSTEM UTSSYS BY AUTOLOG1                         
09:56:57 AUTO LOGON   ***   UTS      USERS = 004  BY  AUTOLOG1  
```
This means UTS is up and running. Start a new 3270 session and enter
```DIAL UTS```
You will get a cryptic
```Name: ```
prompt. Login using **root** with password **root**, remember this is a 3270 terminal so entry is on the bottom line of the screen and you'll need to use **Clear** and **Reset** if you screen fills or locks. 
A few useful commands - remember this is an early V7 UNIX so things are basic.....

|Command|Notes|
|:---------------|:-----|
|/etc/mount|
|df	| no -h flag here|
|ls 	| no columns available
|ps -al |
|man man |man uses uses the 3270 **ned** editor - PF3 to Quit, PF7 forward  screen PF8 back screen
|/etc/cpcmd start all| Issue command to VM370 to start printers etc.
|opr -c A /usr/src/doc/out/beg	| Print the beginners guide NOTE: the printout will appear in the **VM370/io/** directory as one of the print listing files.|
|logoff||
|/etc/shutdown 0|Immediate (clean) shutdown|


<!--stackedit\_data:
eyJoaXN0b3J5IjpbLTgwODk2ODY3MiwtNjgzNTQyMTddfQ==
