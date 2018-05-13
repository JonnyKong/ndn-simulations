# wifi range
# wifi range
rm adhoc-result-new-pattern/syncDuration-range-beacon-10.txt
range_list=(50 60 70 80 90 100 120 140 160)
range_index=0
while(( $range_index < 9))
do
  range=${range_list[range_index]}
  echo "radio range = $range"
  echo "start simulation $index"
  log_file_name="adhoc-log/syncDuration-movepattern.txt"
  NS_LOG='SyncForSleep' ./waf --run "sync-for-sleep-movepattern --pauseTime=0 --lossRate=0 --mobileNodeNum=10 --run=0 --wifiRange=$range --useHeartbeat=false --useBeacon=true --useRetx=false" > ${log_file_name} 2>&1
  result_file_name="adhoc-result-new-pattern/syncDuration-range-beacon-10txt"
  # result_file_name="adhoc-result/test.txt"
  python syncDuration.py ${log_file_name} 10 >> ${result_file_name}
  let "range_index++"
done