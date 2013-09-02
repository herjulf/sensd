#!/bin/bash
# start daemon
DEV=/dev/ttyUSB1
DIR=/tmp/WSN1-GW1

stty -F $DEV sane
mkdir $DIR
# Allow remote reporting
sensd -report -p1234 -R$DIR $DEV 
