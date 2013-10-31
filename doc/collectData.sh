#!/bin/bash
# Script to collect data from pktgen for sensd
# $1 filename where to open the pipe
# use pktgen rx with script output
# echo display script > /proc/net/pktgen/pgrx
# TX should be running. This script does not control it.
# we need to access host without password. So use ssh keys.
# Use screen if you want to use it in background
# screen ./collectData.sh pktgen.dat
#
# To start sensd
# sensd -report -filedev pktgen.dat

host="root@host-pktgen"
#Sensor ID
ID=12345
#interval to collect samples. It resets each time
interval=5

######################## DO NOT MODIFY #####################
############################################################
echo "starting collect data"
pipe=$1

trap "rm -f $pipe" EXIT

if [[ ! -p $pipe ]]; then
    mkfifo $pipe
fi


while (true); do
ssh $host cat /proc/net/pktgen/pgrx | grep G > lastread.tmp
date=`date --rfc-3339=seconds`
PRX=`awk '{print $2}' lastread.tmp`
BRX=`awk '{print $3}' lastread.tmp`
PPS=`awk '{print $5}' lastread.tmp`
MBPS=`awk '{print $6}' lastread.tmp`
echo "&: ID=$ID PRX=$PRX BRX=$BRX PPS=$PPS MBPS=$MBPS " > $pipe
echo "&: ID=$ID PRX=$PRX BRX=$BRX PPS=$PPS MBPS=$MBPS " 
sleep $interval
#reset reading
ssh $host "echo rx_reset > /proc/net/pktgen/pgrx"
done

