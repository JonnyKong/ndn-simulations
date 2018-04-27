import os
import os.path
import numpy as np
import pandas as pd
import statsmodels.api as sm
import matplotlib.pyplot as plt
import math
import re
import sys

data_store = {}
node_num = 40

class DataInfo:
    def __init__(self, birth):
        self.GenerationTime = birth
        self.LastTime = birth
        self.Owner = 1

def cdf_plot(data, name, number, c):
  """
  data: dataset
  name: name on legend
  number: how many pieces are split between min and max
  """
  ecdf = sm.distributions.ECDF(data)
  x = np.linspace(min(data), max(data), number)
  y = ecdf(x)

  #plt.step(x, y, label=name)
  plt.scatter(x, y, alpha=0.5, label=name, color=c)
  #plt.xlim(0, 5)
  plt.show()

# out_file_name = sys.argv[1]
syncDuration = []
out_interest = []
out_data = []

file_name = sys.argv[1]
# file_name = "adhoc-log/syncDuration-movepattern.txt"
file = open(file_name)
for line in file:
    if line.find("microseconds") != -1:
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
            raise AssertionError()
        elif data_info.Owner == node_num:
            cur_sync_duration = data_info.LastTime - data_info.GenerationTime
            cur_sync_duration = float(cur_sync_duration) / 1000000.0
            '''
            if cur_sync_duration >= 25:
                print(data_name)
                print(cur_sync_duration)
            '''
            syncDuration.append(cur_sync_duration)
    if line.find("NFD:") != -1:
      if line.find("m_outInterest") != -1:
        elements = line.split(' ')
        cur_out_interest = elements[4][:-1]
        out_interest.append(float(cur_out_interest))
      elif line.find("m_outData") != -1:
        elements = line.split(' ')
        cur_out_data = elements[4][:-1]
        out_data.append(float(cur_out_data))

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

print(str(float(len(syncDuration)) / float(len(data_store))))
syncDuration = np.array(syncDuration)
print(str(np.mean(syncDuration)))
print(str(np.std(syncDuration)))
print(str(np.mean(np.array(out_interest))))
print(str(np.std(np.array(out_interest))))
print(str(np.mean(np.array(out_data))))
print(str(np.std(np.array(out_data))))

