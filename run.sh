#!/bin/bash
time=0
while(( $time<5 ))
do
  echo "start simulation $time"
  rm snapshot.txt
  ./waf --run sync-for-sleep >/dev/null
  python data_availability2.py >> result2/synctimer2/st150-dt20-node10.txt
  echo "finish simulation $time"
  let "time++"
done
