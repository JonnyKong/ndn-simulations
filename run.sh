# wifi range
rm adhoc-result2/syncDuration-range-flood.txt
range_list=(20 40 60 80 100 150 200 250)
range_index=0
while(( $range_index < 8))
do
  range=${range_list[range_index]}
  echo "radio range = $range"
  index=0
  while(( $index<1 ))
  do
  echo "start simulation $index"
  log_file_name="adhoc-log/syncDuration-movepattern.txt"
  NS_LOG='SyncForSleep' ./waf --run "sync-for-sleep-movepattern --pauseTime=0 --lossRate=0 --run=$index --wifiRange=$range --useHeartbeatFlood=true" > ${log_file_name} 2>&1
  result_file_name="adhoc-result2/syncDuration-range-flood.txt"
  # result_file_name="adhoc-result/test.txt"
  python syncDuration.py ${log_file_name} >> ${result_file_name}
  echo "finish simulation $index"
  let "index++"
  done
  let "range_index++"
done

# wifi range
rm adhoc-result2/syncDuration-range-noflood.txt
range_list=(20 40 60 80 100 150 200 250)
range_index=0
while(( $range_index < 8))
do
  range=${range_list[range_index]}
  echo "radio range = $range"
  index=0
  while(( $index<1 ))
  do
  echo "start simulation $index"
  log_file_name="adhoc-log/syncDuration-movepattern.txt"
  NS_LOG='SyncForSleep' ./waf --run "sync-for-sleep-movepattern --pauseTime=0 --lossRate=0 --run=$index --wifiRange=$range" > ${log_file_name} 2>&1
  result_file_name="adhoc-result2/syncDuration-range-noflood.txt"
  # result_file_name="adhoc-result/test.txt"
  python syncDuration.py ${log_file_name} >> ${result_file_name}
  echo "finish simulation $index"
  let "index++"
  done
  let "range_index++"
done

# loss rate
rm adhoc-result2/syncDuration-loss-beaon.txt
loss_rate_list=(0.0 0.01 0.05 0.1 0.3 0.5)
loss_index=0
while(( $loss_index < 6))
do
  loss_rate=${loss_rate_list[loss_index]}
  echo "loss rate = $loss_rate"
  index=0
  while(( $index<1 ))
  do
  echo "start simulation $index"
  log_file_name="adhoc-log/syncDuration-movepattern.txt"
  NS_LOG='SyncForSleep' ./waf --run "sync-for-sleep-movepattern --pauseTime=0 --run=$index --lossRate=${loss_rate} --wifiRange=60 --useHeartbeat=false --useBeacon=true" > ${log_file_name} 2>&1
  result_file_name="adhoc-result2/syncDuration-loss-beacon.txt"
  python syncDuration.py ${log_file_name} >> ${result_file_name}
  echo "finish simulation $index"
  let "index++"
  done
  let "loss_index++"
done


