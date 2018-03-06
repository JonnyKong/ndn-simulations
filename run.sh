#!/bin/bash
time=0
while(( $time<10 ))
do
  echo "start simulation $time"
  NS_LOG='SyncForSleep' ./waf --run sync-for-sleep > adhoc-log/syncDuration-smallrange-0loss.txt 2>&1
  python syncDuration.py >> adhoc-result/syncDuration-timer3-interval3.txt
  echo "finish simulation $time"
  let "time++"
done
