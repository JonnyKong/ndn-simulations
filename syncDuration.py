import os
import os.path
import numpy as np
import pandas as pd
import statsmodels.api as sm
import matplotlib.pyplot as plt
import math
import re

data_store = {}
node_num = 10

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

syncDuration = []

file_name = "adhoc-log/syncDuration-heartbeat.txt"
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
            print(data_name)
            print(cur_sync_duration)
            syncDuration.append(cur_sync_duration)
            #print(cur_sync_duration)
# cdf_plot(syncDuration, "Synchronization Duration", 100, 'r')

print("result for " + file_name)
print("percentage of complete sync: " + str(float(len(syncDuration)) / float(len(data_store))))
syncDuration = np.array(syncDuration)
print("mean: " + str(np.mean(syncDuration)))
print("min: " + str(np.min(syncDuration)))
print("max: " + str(np.max(syncDuration)))
print("std: " + str(np.std(syncDuration)))




