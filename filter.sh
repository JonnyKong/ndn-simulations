#sed -n '/\s7 YansWifiPhy:/p; /node(1400000)/p' log.txt>log_out.txt
#sed -n '/node(1400000)/p' log.txt>log_out.txt
#drop packet because already in Tx
sed -n '/drop packet because already in/p' log.txt>log_out.txt
