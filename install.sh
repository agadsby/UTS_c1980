#!/bin/sh
#

sudo apt install x3270 fonts-3270 ncat
#
top=`pwd`
echo "Building into $top"
bin=~/bin
download=$top/downloads
utstape=Amdahl_UTS2.aws
#######################
# DOWNLOAD repositories
#######################
cd $download
echo "Starting downloads"
if which hercules >/dev/null ; then
	echo "..Hercules already available. Download skipped"
else
	echo "..Downloading Hercules Helper"
	wget -N -nv https://github.com/wrljet/hercules-helper/archive/master.zip
fi
echo "..Downloading VM370" 
wget -N -nv http://vm370.org/sites/default/files/software/VM370CE.V1.R1.2.zip
echo "..Downloading UTS"
wget -N -nv https://www.mirrorservice.org/sites/tuhs.org/Distributions/IBM/Amdahl_UTS/Amdahl_UTS2.aws.bz2

#####################
# HERCULES 
######################
if which hercules >/dev/null ; then
	echo "Hercules already available. Build skipped"
else
	if [ ! -d $top/hercules ] ; then
		mkdir $top/hercules
	fi
	echo "Making Hercules"
	cd $top/hercules

	unzip $download/master.zip
	./hercules-helper-master/hercules-buildall.sh --auto --flavor=sdl-hyperion >/tmp/hh.$$
	if [ $? -ne 0 ] ; then
		echo "Hercules build failed. See /tmp/hh.$$"
		exit 1
	fi
	cd $top
	echo "After script finishes refresh .profile with . .profile"
	echo "Hercules built"
fi
######################
# VM370
######################

echo "VM370 Install"
cd $download
unzip -q VM370CE.V1.R1.2.zip '*/disks/*'
mv VM370CE.V1.R1.2/disks/* $top/disks
rm -rf VM370CE.V1.R1.2
echo "VM370 DASD installed"

############################
echo "Configuring UTS"
cd $top/tapes
cp $download/Amdahl_UTS2.aws.bz2 . 

echo "..Extracting tape image"
bunzip2 -q $utstape.bz2
chmod ugo-w $utstape
echo "..New DASD volumes in disks-uts"
ls $top/disks-uts/ 
echo "UTS ready for installation using VM370 MAINT user"

##########################
# Configuring bin
##########################
echo "Ensuring $bin has vm.... commands"
if [ ! -d $bin ]
then
	mkdir $bin
fi
cp $top/bin/* $bin/

echo "All Done"
exit 0
