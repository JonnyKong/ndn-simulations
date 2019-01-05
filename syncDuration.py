import os
import os.path
import numpy as np
# import pandas as pd
import statsmodels.api as sm
# import matplotlib.pyplot as plt
import math
import re
import sys

data_store = {}
state_store = {}

class DataInfo:
    def __init__(self, birth):
        self.GenerationTime = birth
        self.LastTime = birth
        self.Owner = 1

# def cdf_plot(data, name, number, c):
#   """
#   data: dataset   
#   name: name on legend
#   number: how many pieces are split between min and max
#   """
#   ecdf = sm.distributions.ECDF(data)
#   x = np.linspace(min(data), max(data), number)
#   y = ecdf(x)

#   #plt.step(x, y, label=name)
#   plt.scatter(x, y, alpha=0.5, label=name, color=c)
#   #plt.xlim(0, 5)
#   plt.show()

# out_file_name = sys.argv[1]
syncDuration = []
stateSyncDuration = []
out_notify_interest = []
out_beacon = []
out_data_interest = []
out_bundled_interest = []
out_data = []
out_ack = []
out_bundled_data = []
collision_rx = []
collision_tx = []
cache_hit = []
cache_hit_special = []
retx_notify_interest = []
retx_data_interest = []
retx_bundled_interest = []
file_name = sys.argv[1]
node_num = int(sys.argv[2]) + 24

# file_name = "adhoc-log/syncDuration-movepattern.txt"
file = open(file_name)
for line in file:
    if line.find("microseconds") != -1:
      if line.find("Store New Data") != -1:
        elements = line.split(' ')
        time = elements[0]
        data_name = elements[-1]
        if data_name not in data_store:
            data_info = DataInfo(int(time))
            data_store[data_name] = data_info
        else:
            data_store[data_name].Owner += 1
            data_store[data_name].LastTime = int(time)
        data_info = data_store[data_name]
        if data_info.Owner > node_num:
            print data_name
            raise AssertionError()
        elif data_info.Owner == node_num:
            cur_sync_duration = data_info.LastTime - data_info.GenerationTime
            cur_sync_duration = float(cur_sync_duration) / 1000000.0
            syncDuration.append(cur_sync_duration)
      elif line.find("Update New Seq") != -1:
        elements = line.split(' ')
        time = elements[0]
        state_name = elements[-1]
        if state_name not in state_store:
            state_info = DataInfo(int(time))
            state_store[state_name] = state_info
        else:
            state_store[state_name].Owner += 1
            state_store[state_name].LastTime = int(time)
        state_info = state_store[state_name]
        if state_info.Owner > node_num:
            raise AssertionError()
        elif state_info.Owner == node_num:
            cur_sync_duration = state_info.LastTime - state_info.GenerationTime
            cur_sync_duration = float(cur_sync_duration) / 1000000.0
            stateSyncDuration.append(cur_sync_duration)
    if line.find("NFD:") != -1:
      if line.find("m_outNotifyInterest") != -1:
        elements = line.split(' ')
        cur_out_notify_interest = elements[4][:-1]
        out_notify_interest.append(float(cur_out_notify_interest))
      if line.find("m_outBeacon") != -1:
        elements = line.split(' ')
        cur_out_beacon = elements[4][:-1]
        out_beacon.append(float(cur_out_beacon))
      elif line.find("m_outDataInterest") != -1:
        elements = line.split(' ')
        cur_out_data_interest = elements[4][:-1]
        out_data_interest.append(float(cur_out_data_interest))
      elif line.find("m_outBundledInterest") != -1:
        elements = line.split(' ')
        cur_out_bundled_interest = elements[4][:-1]
        out_bundled_interest.append(float(cur_out_bundled_interest))
      elif line.find("m_outData") != -1:
        elements = line.split(' ')
        cur_out_data = elements[4][:-1]
        out_data.append(float(cur_out_data))
      elif line.find("m_outAck") != -1:
        elements = line.split(' ')
        cur_out_ack = elements[4][:-1]
        out_ack.append(float(cur_out_ack))
      elif line.find("m_outBundledData") != -1:
        elements = line.split(' ')
        cur_out_bundled_data = elements[4][:-1]
        out_bundled_data.append(float(cur_out_bundled_data))
    if line.find("CollisionRx") != -1:
      collision_rx.append(float(line.split(' ')[-1]))
    if line.find("CollisionTx") != -1:
      collision_tx.append(float(line.split(' ')[-1]))
    if line.find("m_cacheHit") != -1:
      cache_hit.append(float(line.split(' ')[-1]))
    if line.find("m_cacheHitSpecial") != -1:
      cache_hit_special.append(float(line.split(' ')[-1]))
    if line.find("retx_notify_interest") != -1:
      retx_notify_interest.append(float(line.split(' ')[-1]))
    if line.find("retx_data_interest") != -1:
      retx_data_interest.append(float(line.split(' ')[-1]))
    if line.find("retx_bundled_interest") != -1:
      retx_bundled_interest.append(float(line.split(' ')[-1]))

# cdf_plot(syncDuration, "Synchronization Duration", 100, 'r')

# print the unsynced data
'''
print("The un-synced data:")
for data_name in data_store:
    if data_store[data_name].Owner != node_num:
        print(data_name)
'''

'''
with open(out_file_name, 'w') as outFile:
  outFile.write(str(syncDuration) + "\n")
  outFile.write("result for " + file_name + "\n")
  outFile.write("the number of synced data: " + str(len(syncDuration)) + "\n")
  outFile.write("percentage of complete sync: " + str(float(len(syncDuration)) / float(len(data_store))) + "\n")
  syncDuration = np.array(syncDuration)
  outFile.write("mean: " + str(np.mean(syncDuration)) + "\n")
  outFile.write("min: " + str(np.min(syncDuration)) + "\n")
  outFile.write("max: " + str(np.max(syncDuration)) + "\n")
  outFile.write("std: " + str(np.std(syncDuration)) + "\n")
'''

#print(data_store)
'''
print(str(syncDuration))
print("result for " + file_name)
print("the number of synced data: " + str(len(syncDuration)))
print("percentage of complete sync: " + str(float(len(syncDuration)) / float(len(data_store))))
syncDuration = np.array(syncDuration)
print("mean: " + str(np.mean(syncDuration)))
print("min: " + str(np.min(syncDuration)))
print("max: " + str(np.max(syncDuration)))
print("std: " + str(np.std(syncDuration)))
print("average outInterest: " + str(np.mean(np.array(out_interest))))
print("std outInterest: " + str(np.std(np.array(out_interest))))
print("average outData: " + str(np.mean(np.array(out_data))))
print("std outData: " + str(np.std(np.array(out_data))))
'''

print("data availability = " + str(float(len(syncDuration)) / float(len(data_store))))
print(str(syncDuration))
syncDuration = np.array(syncDuration)
print("sync delay = " + str(np.mean(syncDuration)))
print("out notify interest = " + str(np.sum(np.array(out_notify_interest))))
print("out beacon = " + str(np.sum(np.array(out_beacon))))
print("out data interest = " + str(np.sum(np.array(out_data_interest))))
print("out bundled interest = " + str(np.sum(np.array(out_bundled_interest))))
print("out data = " + str(np.sum(np.array(out_data))))
print("out ack = " + str(np.sum(np.array(out_ack))))
print("out bundled data = " + str(np.sum(np.array(out_bundled_data))))
# print("collision rx = " + str(np.mean(np.array(collision_rx))))
# print("collision tx = " + str(np.mean(np.array(collision_tx))))
print("cache hit = " + str(np.sum(np.array(cache_hit))))
print("cache hit special = " + str(np.sum(np.array(cache_hit_special))))
print("retx_notify_interest = " + str(np.sum(np.array(retx_notify_interest))))
print("retx_data_interest = " + str(np.sum(np.array(retx_data_interest))))
print("retx_bundled_interest = " + str(np.sum(np.array(retx_bundled_interest))))
print("state sync duration = " + str(np.mean(stateSyncDuration)))

print("number of data available = " + str(syncDuration.size))
print("number of data produced = " + str(len(data_store)))