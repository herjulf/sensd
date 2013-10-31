#!/bin/bash
# Script to collect data from pktgen for sensd
echo "starting collect data"
#exec > >(tee $1)
pipe=$1

trap "rm -f $pipe" EXIT

if [[ ! -p $pipe ]]; then
    mkfifo $pipe
fi

host="root@host-pktgen"
while (true); do
ssh $host cat /proc/net/pktgen/pgrx | grep G > lastread.tmp
date=`date --rfc-3339=seconds`
PRX=`awk '{print $2}' lastread.tmp`
BRX=`awk '{print $3}' lastread.tmp`
PPS=`awk '{print $5}' lastread.tmp`
MBPS=`awk '{print $6}' lastread.tmp`
echo "&: ID=12345 PRX=$PRX BRX=$BRX PPS=$PPS MBPS=$MBPS " > $pipe
echo "&: ID=12345 PRX=$PRX BRX=$BRX PPS=$PPS MBPS=$MBPS " 
sleep 5
#reset reading
ssh $host "echo rx_reset > /proc/net/pktgen/pgrx"
done

