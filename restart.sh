#!/bin/sh

#rm -rf /home/sheep2
/etc/init.d/sheepdog stop
/etc/init.d/corosync restart
sheep /home/sheep2 -l level=debug
sleep 1
collie node list

