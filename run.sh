# loss rate
rm adhoc-result-new-pattern/syncDuration-loss-beacon-20.txt
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
  NS_LOG='SyncForSleep' ./waf --run "sync-for-sleep-movepattern --pauseTime=0 --run=$index --mobileNodeNum=20 --lossRate=${loss_rate} --wifiRange=60 --useHeartbeat=false --useBeacon=true --useRetx=false" > ${log_file_name} 2>&1
  result_file_name="adhoc-result-new-pattern/syncDuration-loss-beacon-20.txt"
  python syncDuration.py ${log_file_name} 20 >> ${result_file_name}
  echo "finish simulation $index"
  let "index++"
  done
  let "loss_index++"
done

rm adhoc-result-new-pattern/syncDuration-loss-retx-20.txt
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
  NS_LOG='SyncForSleep' ./waf --run "sync-for-sleep-movepattern --pauseTime=0 --run=$index --mobileNodeNum=20 --lossRate=${loss_rate} --wifiRange=60 --useHeartbeat=false --useBeacon=false --useRetx=true" > ${log_file_name} 2>&1
  result_file_name="adhoc-result-new-pattern/syncDuration-loss-retx-20.txt"
  python syncDuration.py ${log_file_name} 20 >> ${result_file_name}
  echo "finish simulation $index"
  let "index++"
  done
  let "loss_index++"
done