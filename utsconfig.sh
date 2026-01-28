#!/bin/sh
#
# Very simple script to aoutomate the startup of VM370 and prime the card reader and tape
# ready for VM370 MAINT user to load UTS image
# Once sucessfully run user can "DIAL UTS" using VM370 terminal
TOP="${VMHOME:-$HOME/VM}"
cd $TOP
echo "Start VM370 using a another session"
echo -n "Hit <Enter> when done : "
read line
echo "Enabling spool devices on VM370"
vmcmd "/start all"
sleep 1
echo "Send UTS Config Cards to MAINT via RDR"
vmrdr -v MAINT cards/adduts_exec.cards
sleep 1
echo "Loading UTS installation tape on 0480"
vmcmd devinit 0480 tapes/Amdahl_UTS2.aws
sleep 1
echo "Using a 3270 window\n
LOGIN MAINT CPCMS\n
READCARD ADDUTS EXEC \n
ADDUTS"
echo "Once succesfully run use another 3270 window to DIAL UTS"
echo "login using root / root"
echo "To shutdown use 'vmcmd / shutdown' followed by 'vmcmd exit'"
echo "You can view the 370 hardware console using 'vmconsole'"
