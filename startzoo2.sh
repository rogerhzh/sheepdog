collie cluster shutdown
/etc/init.d/sheepdog stop
sheep /home/sheep8  -c zookeeper:dog110:2181 -l level=debug
sleep 1
collie node list
