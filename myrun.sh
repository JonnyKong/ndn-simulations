#!/bin/bash

NS_LOG='SyncForSleep' ./waf --run "sync-for-sleep-movepattern --pauseTime=0 --run=0 --mobileNodeNum=20 --lossRate=0 --wifiRange=60 --useBeacon=true --useRetx=false"