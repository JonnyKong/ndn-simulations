#!/bin/bash

loss_rate_list=(0.0 0.01 0.05 0.1 0.3 0.5)
wifi_range_list=(40 60 80 100 120 140 160) 

run_loss_rate() {
    local LOSS_RATE=$1
    local RESULT_DIR=$2
    echo "Starting simulation: Loss rate = ${LOSS_RATE} ..."
    NS_LOG='SyncForSleep' ./waf --run "sync-for-sleep-movepattern --pauseTime=0 \
        --run=0 --mobileNodeNum=20 --lossRate=${LOSS_RATE} --wifiRange=60" \
        > ${RESULT_DIR}/raw/loss_rate_${LOSS_RATE}.txt 2>&1
    python syncDuration.py ${RESULT_DIR}/raw/loss_rate_${LOSS_RATE}.txt 20 \
        > ${RESULT_DIR}/loss_rate_${LOSS_RATE}.txt
    pids="$pids $!"
}

run_wifi_range() {
    local WIFI_RANGE=$1
    local RESULT_DIR=$2
    echo "Starting simulation: Wifi range = ${WIFI_RANGE} ..."
    NS_LOG='SyncForSleep' ./waf --run "sync-for-sleep-movepattern --pauseTime=0 \
        --run=0 --mobileNodeNum=20 --lossRate=0 --wifiRange=${WIFI_RANGE} "\
        > ${RESULT_DIR}/raw/wifi_range_${WIFI_RANGE}.txt 2>&1
    python syncDuration.py ${RESULT_DIR}/raw/wifi_range_${WIFI_RANGE}.txt 20 \
        > ${RESULT_DIR}/wifi_range_${WIFI_RANGE}.txt
    pids="$pids $!"
}

summarize_wifi_range_result() {
    local RESULT_DIR=$1
    local FILENAME=wifi_range.txt
    
    # Interest for missing data
    echo "Interest for missing data:" >> ${RESULT_DIR}/${FILENAME}
    for WIFI_RANGE in "${wifi_range_list[@]}"; do
        echo -n "Wifi range = ${WIFI_RANGE}  " >> ${RESULT_DIR}/${FILENAME}
        cat ${RESULT_DIR}/wifi_range_${WIFI_RANGE}.txt \
            | grep "out data interest" >> ${RESULT_DIR}/${FILENAME}
    done

    # Transmitted data
    echo "Transmitted data (number of packets):" >> ${RESULT_DIR}/${FILENAME}
    for WIFI_RANGE in "${wifi_range_list[@]}"; do
        echo -n "Wifi range = ${WIFI_RANGE}  " >> ${RESULT_DIR}/${FILENAME}
        cat ${RESULT_DIR}/wifi_range_${WIFI_RANGE}.txt \
            | grep "out" | awk '{sum+=$NF} END {print sum}' \
            >> ${RESULT_DIR}/${FILENAME}
    done

    # Sync delay
    echo "Sync delay:" >> ${RESULT_DIR}/${FILENAME}
    for WIFI_RANGE in "${wifi_range_list[@]}"; do
        echo -n "Wifi range = ${WIFI_RANGE}  " >> ${RESULT_DIR}/${FILENAME}
        cat ${RESULT_DIR}/wifi_range_${WIFI_RANGE}.txt \
            | grep "sync delay" >> ${RESULT_DIR}/${FILENAME}
    done
}

run() {
    local RESULT_DIR=result/$(date -I)
    pids=""
    rm -rf $RESULT_DIR && mkdir -p $RESULT_DIR/raw
    for i in "${loss_rate_list[@]}"; do
        run_loss_rate $i $RESULT_DIR &
    done
    for i in "${wifi_range_list[@]}"; do
        run_wifi_range $i $RESULT_DIR &
    done    
    wait $pids

    summarize_wifi_range_result $RESULT_DIR
}

run