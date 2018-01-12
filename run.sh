#!/bin/bash
echo "compiling ns-3"
cd ..
cd ns-3
./waf >/dev/null
./waf install >/dev/null
cd ..
cd syncforsleep-simulations
./waf configure --debug >/dev/null
echo "finish compiling ns-3"

time=0
while(( $time<1 ))
do
  echo "start simulation $time"
  rm snapshot.txt
  ./waf --run sync-for-sleep 
  python data_availability2.py
  echo "finish simulation $time"
  let "time++"
done

#>/dev/null