import os
import os.path
import numpy as np
import pandas as pd
import statsmodels.api as sm
import matplotlib.pyplot as plt
import math

'''
file = open("result.txt")
sample_num = 5
line_idx = -1
node_id = -1
data_availability = []
ave_sleeping = []
std_sleeping = []
for line in file:
  line_idx += 1
  if line_idx % (sample_num * 2) == 0:
  	node_id += 1
  	data_availability.append(0.0)
  	ave_sleeping.append(0.0)
  	std_sleeping.append(0.0)
  if line_idx % 2 == 0:
  	data_availability[node_id] += float(line)
  else:
  	cur_sleeping_dist = np.array(map(int, line.split(",")))
  	ave_sleeping[node_id] += np.mean(cur_sleeping_dist)
  	std_sleeping[node_id] += np.std(cur_sleeping_dist)
  if line_idx % (sample_num * 2) == 9:
  	data_availability[node_id] /= sample_num
  	ave_sleeping[node_id] /= sample_num
  	std_sleeping[node_id] /= sample_num
'''

node_num = np.array([5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15])

'''
print data_availability
print ave_sleeping
print std_sleeping
'''

def plot_availability():
  plt.plot(node_num, data_availability)
  plt.title('Data Availability - #(Node in Group)')
  plt.xlabel("The number of nodes in group")
  plt.ylabel("Data Availability")
  plt.ylim((0.85, 1.0))
  plt.show()

'''
def plot_sleeping_node_num():
  plt.errorbar(node_num, ave_sleeping, std_sleeping, linestyle='None', marker='^')
  plt.title("Average Sleeping Time - #(Node in Group)")
  plt.xlabel("The number of nodes in group")
  plt.ylabel("Avarage Sleeping Time")
  plt.show()
'''

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

def plot_sync_delay():
  file = open("result-10-node-0-packet-loss-oneack.txt")
  first_syncack_delay = []
  last_syncack_delay = []
  data_availability = 0.0
  line_idx = -1
  for line in file:
    line_idx += 1
    if line_idx % 4 == 0:
      data_availability += float(line)
    elif line_idx % 4 == 1:
      first_syncack_delay.append(float(line))
    elif line_idx % 4 == 2:
      last_syncack_delay.append(float(line))
  cdf_plot(first_syncack_delay, "First SyncACK Interest Delay", 100, 'r')
  cdf_plot(last_syncack_delay, "Last SyncACK Interest Delay", 100, 'g')
  data_availability /= 70
  '''
  plt.scatter([1,2,3], [1,2,3], alpha=0.5, label="First SyncACK Delay", color='r')
  plt.scatter([1,2,3], [1,2,3], alpha=0.5, label="Last SyncACK Delay", color='g')
  '''
  plt.xlabel("SyncACK Delay (milliseconds)")
  plt.ylabel("CDF")
  plt.legend(bbox_to_anchor=(0.4, 0.15), loc=2, borderaxespad=0.)
  plt.ylim((0.0, 1.0))
  plt.xlim(0.0, 400.0)
  plt.show()
  print data_availability

def calculate_average():
  file = open("template.txt")
  line_idx = -1
  sample_num = 10
  data_availability = 0.0
  first_syncack_delay = 0.0
  last_syncack_delay = 0.0
  for line in file:
    line_idx += 1
    if (line_idx % 4 == 0):
      data_availability += float(line)
    elif (line_idx % 4 == 1):
      first_syncack_delay += float(line)
    elif (line_idx % 4 == 2):
      last_syncack_delay += float(line)
  data_availability /= 10.0
  first_syncack_delay /= 10.0
  last_syncack_delay /= 10.0
  file = open("timer-change-10-node.txt", "a+")
  file.write("%s,%s,%s\n" % (data_availability, first_syncack_delay, last_syncack_delay))

def plot_timer_sync_delay():
  file = open("timer-change-10-node.txt")
  timer = range(40, 71, 2)
  first_syncack_delay = []
  last_syncack_delay = []
  for line in file:
    data = np.array(map(float, line.split(",")))
    first_syncack_delay.append(data[1])
    last_syncack_delay.append(data[2])
  plt.plot(timer, first_syncack_delay, alpha=0.5, color='r', label='First SyncACK Interest Delay')
  plt.plot(timer, last_syncack_delay, alpha=0.5, color='g', label='Last SyncACK Interest Delay')
  plt.xlabel("Timer Threshold (milliseconds)")
  plt.ylabel("SyncACK Delay (milliseconds)")
  plt.legend(bbox_to_anchor=(0.4, 0.15), loc=2, borderaxespad=0.)
  plt.ylim((30.0, 110.0))
  plt.show()

def get_average_sleeping_time(filename):
  file = open(filename)
  line_idx = -1
  ave_sleeping = 0.0
  for line in file:
    line_idx += 1
    if line_idx % 4 == 3:
      sleeping = line[1:-2]
      sleeping_dist = np.array(map(int, sleeping.split(",")))
      #print sleeping_dist
      ave_sleeping += np.mean(sleeping_dist)
  ave_sleeping /= 10
  print ave_sleeping

def get_average_data_availability(filename):
  file = open(filename)
  data_availability = 0.0
  line_idx = -1
  for line in file:
    line_idx += 1
    if line_idx % 4 == 0:
      data_availability += float(line)
  data_availability /= 10
  print data_availability

def plot_syncduration_dataavailability(filename):
  file = open(filename)
  sync_duration = []
  data_availability = []
  sleeping_time = []
  for line in file:
    data = np.array(map(float, line.split(",")))
    sync_duration.append(int(data[0]))
    data_availability.append(data[1])
    sleeping_time.append(float(data[2]) / 2100000.0)
  #plt.plot(sync_duration, data_availability, alpha=0.5, color='r')
  plt.plot(sync_duration, sleeping_time, alpha=0.5, color='g', label='Sleeping Time')
  plt.xlabel("Sync Duration Threshold (milliseconds)")
  plt.ylabel("Sleeping Time Percentage")
  #plt.ylim(0.6, 0.65)
  plt.xlim(50, 450)
  plt.show()

def calculate_suppression(filename):
  file = open(filename)
  suppression = 0.0
  result = 0.0
  line_idx = -1
  for line in file:
    line_idx += 1
    if line_idx % 4 == 1:
      suppression = (float(line))
    elif line_idx % 4 == 2:
      result += suppression / (float(line))
  print(result / 10.0)

def plot_timer_collision():
  timer = range(20, 71, 5)
  collision_num = [101.7, 81.6, 69.4, 52.4, 48.4, 40.9, 40.6, 37.7, 36.6, 32.3, 27.7]
  plt.plot(timer, collision_num, alpha=0.5, color='r')
  plt.xlabel("Timer Threshold (milliseconds)")
  plt.ylabel("Collision Occurance")
  plt.show()
    

if __name__ == "__main__":
  #plot_availability()
  # plot_sync_delay()
  # calculate_average()
  # plot_timer_sync_delay()
  # get_average_sleeping_time()
  #get_average_data_availability("sleep-duration-50.txt")
  # get_average_sleeping_time("sleep-duration-50.txt")
  plot_syncduration_dataavailability("result/syncDuration-dataAvailability.txt")
  #calculate_suppression("result/suppression/suppression2.txt")
  #plot_timer_collision()
  #calculate_collision("result/suppression/suppression.txt")

  	
