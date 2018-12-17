#!/bin/bash

LOG_DIR=result2

loss_rate_list=(0.0 0.01 0.05 0.1 0.3 0.5)
pids=()

for loss in ${loss_rate_list[@]}; do
    echo "Starting simulating (loss_rate=${loss}) ... "
    NS_LOG='SyncForSleep' ./waf --run "sync-for-sleep-movepattern --pauseTime=0 \
        --run=0 --mobileNodeNum=20 --lossRate=${loss} --wifiRange=60 --useBeacon=true \
        --useRetx=false" > ${LOG_DIR}/${loss}.txt 2>&1 &
    if [ $? -ne 0 ]; then
        echo "Terminating ... "
        exit -1
    else
        pids+=($!)
        echo "Starting process ${#pids[@]}"
    fi
done

for pid in ${pids[@]};do
    wait $pid
    echo "Finishing process ... "
done

for loss in ${loss_rate_list[@]}; do
    echo "Analyzing results (loss_rate=${loss}) ... "    
    python syncDuration.py ${LOG_DIR}/${loss}.txt 20 >> ${LOG_DIR}/${loss}_result.txt &
    if [ $? -ne 0 ]; then
        echo "Terminating ... "
        exit -1
    fi;
done

echo "DONE!"