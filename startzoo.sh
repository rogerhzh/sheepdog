/etc/init.d/sheepdog stop
#rm -rf /home/sheep4
sheep /home/sheep4  -c zookeeper:dog110:2181 -l level=debug
sleep 1
collie node list
