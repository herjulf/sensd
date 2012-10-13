#!/bin/bash
# start daemon
DEV=/dev/ttyUSB1
DIR=/tmp/WSN1-GW1

stty -F $DEV sane
mkdir $DIR
sensd  -R$DIR $DEV 
