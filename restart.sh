#!/bin/sh

rm -rf /home/sheep2
/etc/init.d/corosync start
sheep /home/sheep2 -l level=debug
collie node list

