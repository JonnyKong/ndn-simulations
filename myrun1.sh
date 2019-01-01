#!/bin/bash

rm -f result*

NS_LOG='SyncForSleep' ./waf --run "sync-for-sleep-movepattern --pauseTime=0 \
        --run=0 --mobileNodeNum=20 --lossRate=0 --wifiRange=60 --useBeacon=true \
        --useRetx=false" >> result.txt 2>&1

python syncDuration.py result.txt 20 >> result_a.txt

cat result.txt | grep "node(0)" >> result_node_0.txt
cat result.txt | grep "node(1)" >> result_node_1.txt
cat result.txt | grep "node(40)" >> result_node_40.txt