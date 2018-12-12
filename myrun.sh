#!/bin/bash

LOG_FILE="log.txt"
RESULT_FILE="result.txt"

echo "Starting simulation ... "
NS_LOG='SyncForSleep' ./waf --run "sync-for-sleep-movepattern \--pauseTime=0 \
    --run=0 --mobileNodeNum=20 --lossRate=0 --wifiRange=60 --useBeacon=true \
    --useRetx=false" > ${LOG_FILE} 2>&1
if [ $? -ne 0 ]; then
    echo "Terminating ... "
    exit -1;
fi;

echo "Analyzing results ... "
python3 syncDuration.py ${LOG_FILE} 20 >> ${RESULT_FILE}
if [ $? -ne 0 ]; then
    echo "Terminating ... "
fi;

echo "DONE!"