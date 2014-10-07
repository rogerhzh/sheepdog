/etc/init.d/sheepdog stop
#rm -rf /home/sheep4
mkdir /home/sheep_cache
sheep /home/sheep8  -c zookeeper:dog110:2181 -w size=20G,dir=/home/sheep_cache
sleep 1
collie node list
