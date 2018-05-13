import os
import os.path
import numpy as np
import pandas as pd
import statsmodels.api as sm
import matplotlib.pyplot as plt
import math

pause = [0, 30, 60, 120, 300, 600, 900]
color = ['red', 'orange', 'yellow', 'springgreen', 'cyan', 'blue', 'mediumpurple', 'pink']
loss = [0.0, 0.01, 0.05, 0.1, 0.3, 0.5]
#wifi_range = [40, 60, 80, 100, 150, 200, 250]
wifi_range = [50, 60, 70, 80, 90, 100, 120, 140, 160]
heartbeat = [3, 5, 10, 15, 20, 25, 30, 40]
node_num = [10, 15, 20, 25, 30]
log_line = 18

def rotate(list):
  result = []
  for i in range(len(list)):
    result.append(list[(-1) * (i + 1)])
  return result

def cdf_plot(data, label, number, c):
  """
  data: dataset
  name: name on legend
  number: how many pieces are split between min and max
  """
  ecdf = sm.distributions.ECDF(data)
  x = np.linspace(min(data), max(data), number)
  y = ecdf(x)

  #plt.step(x, y, label=name)
  plt.scatter(x, y, alpha=0.5, label=label, color=c, s=8)
  #plt.xlim(0, 5)

def get_cdf(filename, list, type):
  file = open(filename)
  idx = 0
  data_availability = []
  delay = []
  state_delay = []
  out_notify_interest = []
  out_beacon = []
  out_data_interest = []
  out_bundled_interest = []
  out_data = []
  out_ack = []
  out_bundled_data = []
  collision = []
  cache_hit = []
  retx_notify_interest = []
  retx_data_interest = []
  retx_bundled_interest = []
  cur_collision = 0
  for line in file:
    if idx % log_line == 0:
      data_availability.append(float(line.split(" ")[-1]))
    if idx % log_line == 1:
      sync_delay = np.array(map(float, line[1:-2].split(",")))
      # cdf_plot(sync_delay, type + " = " + str(list[idx / log_line]), 1000, color[idx / log_line])
    elif idx % log_line == 2:
      delay.append(float(line.split(" ")[-1]))
    elif idx % log_line == 3:
      out_notify_interest.append(float(line.split(" ")[-1]))
    elif idx % log_line == 4:
      out_beacon.append(float(line.split(" ")[-1]))
    elif idx % log_line == 5:
      out_data_interest.append(float(line.split(" ")[-1]))
    elif idx % log_line == 6:
      out_bundled_interest.append(float(line.split(" ")[-1]))
    elif idx % log_line == 7:
      out_data.append(float(line.split(" ")[-1]))
    elif idx % log_line == 8:
      out_ack.append(float(line.split(" ")[-1]))
    elif idx % log_line == 9:
      out_bundled_data.append(float(line.split(" ")[-1]))
    elif idx % log_line == 10:
      cur_collision += float(line.split(" ")[-1])
    elif idx % log_line == 11:
      cur_collision += float(line.split(" ")[-1])
      collision.append(cur_collision)
      cur_collision = 0
    elif idx % log_line == 12:
      cache_hit.append(float(line.split(" ")[-1]))
    elif idx % log_line == 14:
      retx_notify_interest.append(float(line.split(" ")[-1]))
    elif idx % log_line == 15:
      retx_data_interest.append(float(line.split(" ")[-1]))
    elif idx % log_line == 16:
      retx_bundled_interest.append(float(line.split(" ")[-1]))
    elif idx % log_line == 17:
      state_delay.append(float(line.split(" ")[-1]))
    idx += 1

  '''
  # plot cdf
  print(data_availability)
  plt.title("CDF of Sync Delay of Different " + type)
  plt.legend(bbox_to_anchor=(0.60, 0.55), loc=2, borderaxespad=0.)
  plt.show()
  '''

  '''
  xticks = ['250', '200', '150', '100', '80', '60', '40', '20']
  plt.bar(np.arange(len(wifi_range)) + 1, data_availability_list, width=0.35, align="center", color="c", alpha=0.8)
  plt.xticks(np.arange(len(wifi_range)) + 1, xticks, size="small")
  plt.ylim(0.0, 1.2)
  plt.xlabel("Wifi Range")
  plt.ylabel("Data Availability")
  plt.title("Data Availability - Wifi Range")
  plt.show()
  '''

  '''
  plt.plot(list, delay, alpha=0.5, color='royalblue', linewidth=2.0)
  plt.xlabel(type)
  plt.ylabel("Sync Delay")
  plt.title("Sync Delay - " + type)
  plt.show()
  '''

  '''
  plt.plot(list, out_data_interest, alpha=0.5, color='orange', label="#(Out Data Interest)", linewidth=2.0)
  plt.plot(list, out_data, alpha=0.5, color='royalblue', label="#(Out Data)", linewidth=2.0)
  plt.plot(list, retx_data_interest, '--', alpha=0.5, color='orange', label="#(Retx Data Interest)", linewidth=2.0)
  plt.xlabel(type)
  plt.ylabel("Out Data/Data-Interest")
  plt.title("Out Data/Data-Interest - " + type)
  plt.legend(bbox_to_anchor=(0.1, 0.8), loc=2, borderaxespad=0.)
  plt.show() 

  plt.plot(list, out_notify_interest, alpha=0.5, color='red', linewidth=2.0)
  plt.plot(list, retx_notify_interest, '--', alpha=0.5, color='red', linewidth=2.0)
  plt.xlabel(type)
  plt.ylabel("Out Notify-Interest")
  plt.title("Out Notify-Interest - " + type)
  plt.show()

  plt.plot(list, out_bundled_interest, alpha=0.5, color='orange', label="#(Out Bundled Interest)", linewidth=2.0)
  plt.plot(list, out_bundled_data, alpha=0.5, color='royalblue', label="#(Out Bundled Data)", linewidth=2.0)
  plt.xlabel(type)
  plt.ylabel("Out Bundled Interest/Data")
  plt.title("Out Bundled Interest/Data - " + type)
  plt.legend(bbox_to_anchor=(0.60, 0.55), loc=2, borderaxespad=0.)
  plt.show()

  plt.plot(list, cache_hit, alpha=0.5, color='red', linewidth=2.0)
  plt.xlabel(type)
  plt.ylabel("Number of Local Cache Hit for Data Interest")
  plt.title("Number of Local Cache Hit for Data Interest - " + type)
  plt.show()

  plt.plot(list, collision, alpha=0.5, color='royalblue', linewidth=2.0)
  plt.xlabel(type)
  plt.ylabel("Collision")
  plt.title("Collision - " + type)
  plt.show()
  '''

  out_interest = [i + j for i, j in zip(out_notify_interest, out_data_interest)]
  out_interest = [i + j for i, j in zip(out_interest, out_bundled_interest)]
  out_total_data = [i + j for i, j in zip(out_data, out_bundled_data)]
  return state_delay, delay, collision, out_interest, out_notify_interest, out_data_interest, out_bundled_interest, out_total_data, out_data, out_bundled_data, out_beacon, out_ack

def get_cdf_fast_resync():
  file_name = "adhoc-result/syncDuration-fastresync.txt"
  file = open(file_name)
  idx = 0
  data_availability_list = []
  delay = []
  std = []
  out_interest = []
  std_out_interest = []
  out_data = []
  std_out_data = []
  for line in file:
    if idx % log_line == 0:
      sync_delay = np.array(map(float, line[1:-2].split(",")))
      if idx == 0:
        cdf_plot(sync_delay, "Use Bundling Fast Resync", 1000, color[idx / log_line])
      else:
        cdf_plot(sync_delay, "Not Use Bundling Fast Resync", 1000, color[idx / log_line])
    elif idx % log_line == 3:
      data_availability_list.append(float(line.split(" ")[-1]))
    elif idx % log_line == 4:
      delay.append(float(line.split(" ")[-1]))
    elif idx % log_line == 7:
      std.append(float(line.split(" ")[-1]))
    elif idx % log_line == 8:
      out_interest.append(float(line.split(" ")[-1]))
    elif idx % log_line == 9:
      std_out_interest.append(float(line.split(" ")[-1]))
    elif idx % log_line == 10:
      out_data.append(float(line.split(" ")[-1]))
    elif idx % log_line == 11:
      std_out_data.append(float(line.split(" ")[-1]))
    idx += 1
  plt.title("CDF of sync delay")
  plt.ylim(0)
  plt.xlim(-50)
  plt.legend(bbox_to_anchor=(0.55, 0.45), loc=2, borderaxespad=0.)
  plt.show()

  xticks = ['Use Fast Resync', 'Not Use Fast Resync']
  plt.bar(np.arange(2) + 1, delay, width=0.35, align="center", color="c", alpha=0.8)
  plt.xticks(np.arange(2) + 1, xticks, size="small")
  plt.ylabel("Sync Delay")
  plt.show()

  plt.bar(np.arange(2) + 1, out_interest, width=0.35, align="center", color="orange", alpha=0.8)
  plt.bar(np.arange(2) + 1, out_data, width=0.35, align="center", color="springgreen", alpha=0.8)
  plt.xticks(np.arange(2) + 1, xticks, size="small")
  plt.ylabel("Packet Number")
  plt.show()

def get_cdf_heartbeat():
  file_name = "adhoc-result/syncDuration-heartbeat.txt"
  file = open(file_name)
  idx = 0
  data_availability_list = []
  delay = []
  std = []
  out_interest = []
  std_out_interest = []
  out_data = []
  std_out_data = []
  for line in file:
    if idx % log_line == 0:
      sync_delay = np.array(map(float, line[1:-2].split(",")))
      cdf_plot(sync_delay, "heartbeat = " + str(heartbeat[idx / log_line]), 1000, color[idx / log_line])
    elif idx % log_line == 3:
      data_availability_list.append(float(line.split(" ")[-1]))
    elif idx % log_line == 4:
      delay.append(float(line.split(" ")[-1]))
    elif idx % log_line == 7:
      std.append(float(line.split(" ")[-1]))
    elif idx % log_line == 8:
      out_interest.append(float(line.split(" ")[-1]))
    elif idx % log_line == 9:
      std_out_interest.append(float(line.split(" ")[-1]))
    elif idx % log_line == 10:
      out_data.append(float(line.split(" ")[-1]))
    elif idx % log_line == 11:
      std_out_data.append(float(line.split(" ")[-1]))
    idx += 1
  plt.title("CDF of sync delay of different heartbeat interval")
  plt.ylim(0)
  plt.xlim(-50)
  plt.legend(bbox_to_anchor=(0.60, 0.55), loc=2, borderaxespad=0.)
  plt.show()

  xticks = ['3', '5', '10', '15', '20', '25', '30', '40']
  plt.bar(np.arange(len(heartbeat)) + 1, data_availability_list, width=0.35, align="center", color="c", alpha=0.8)
  plt.xticks(np.arange(len(heartbeat)) + 1, xticks, size="small")
  plt.ylim(0.0, 1.2)
  plt.xlabel("Wifi Range")
  plt.ylabel("Data Availability")
  plt.title("Data Availability - Wifi Range")
  plt.show()

  plt.errorbar(heartbeat, delay, std, linestyle='None', marker='^')
  plt.plot(heartbeat, delay, alpha=0.5, color='b')
  plt.xlabel("Heartbeat Interval")
  plt.ylabel("Sync Delay")
  plt.title("Sync Delay - Heartbeat Interval")
  plt.xlim(0, 50)
  plt.show()

  #plt.errorbar(heartbeat, out_interest, std_out_interest, linestyle='None', marker='^', color='b')
  plt.plot(heartbeat, out_interest, alpha=0.5, color='b', label="#(outInterest)")
  #plt.errorbar(heartbeat, out_data, std_out_data, linestyle='None', marker='^', color='r')
  plt.plot(heartbeat, out_data, alpha=0.5, color='r', label="#(outData)")
  plt.xlabel("Heartbeat Interval")
  plt.ylabel("Packet Number")
  plt.xlim(0, 50)
  plt.title("Packet Number - Heartbeat Interval")
  plt.legend(bbox_to_anchor=(0.1, 0.8), loc=2, borderaxespad=0.)
  plt.show()  

def get_ave(item_list, type, name):
  data_availability = []
  delay = []
  std_delay = []
  out_interest = []
  std_out_interest = []
  out_data = []
  std_out_data = []

  for item in item_list:
    file_name = "adhoc-result/syncDuration-" + type + str(item) + ".txt"
    file = open(file_name)
    idx = 0
    cur_data_availability = []
    cur_delay = []
    cur_std_delay = []
    cur_out_interest = []
    cur_std_out_interest = []
    cur_out_data = []
    cur_std_out_data = []

    for line in file:
      if idx % 7 == 0:
        cur_data_availability.append(float(line))
      elif idx % 7 == 1:
        value = float(line)
        if not math.isnan(value):
          cur_delay.append(value)
      elif idx % 7 == 2:
        value = float(line)
        if not math.isnan(value):
          cur_std_delay.append(value)
      elif idx % 7 == 3:
        value = float(line)
        if not math.isnan(value):
          cur_out_interest.append(value)
      elif idx % 7 == 4:
        value = float(line)
        if not math.isnan(value):
          cur_std_out_interest.append(value)
      elif idx % 7 == 5:
        value = float(line)
        if not math.isnan(value):
          cur_out_data.append(value)
      elif idx % 7 == 6:
        value = float(line)
        if not math.isnan(value):
          cur_std_out_data.append(value)
      idx += 1
    idx /= 7
    data_availability.append(np.mean(np.array(cur_data_availability)))
    delay.append(np.mean(np.array(cur_delay)))
    std_delay.append(np.mean(np.array(cur_std_delay)))
    out_interest.append(np.mean(np.array(cur_out_interest)))
    std_out_interest.append(np.mean(np.array(cur_std_out_interest)))
    out_data.append(np.mean(np.array(cur_out_data)))
    std_out_data.append(np.mean(np.array(cur_std_out_data)))

  x = np.arange(len(item_list)) + 1
  xticks = [str(item) for item in item_list]
  # plot bar for data availability
  plt.bar(x, data_availability, width=0.35, align="center", color="c", alpha=0.8)
  plt.xticks(x, xticks, size="small")
  plt.ylim(0.0, 1.2)
  plt.xlabel(name)
  plt.ylabel("Data Availability")
  plt.title("Data Availability - " + name)
  plt.show()

  if type == "fastresync":
    print(delay)
    plt.bar(x, delay, width=0.35, align="center", color="c", alpha=0.8)
    plt.xticks(x, xticks, size="small")
    plt.ylim(0.0, 1.2)
    plt.xlabel(name)
    plt.ylabel("Sync Delay")
    plt.title("Sync Delay - " + name)
    plt.show()

    plt.bar(x, out_interest, width=0.35, align="center", color="r", alpha=0.8)
    plt.bar(x, out_data, width=0.35, align="center", color="b", alpha=0.8)
    plt.xticks(x, xticks, size="small")
    plt.xlabel(name)
    plt.ylabel("Packet Number")
    plt.title("Packet Number - " + name)
    plt.show() 
  else:
    # plot delay
    plt.plot(x, delay, color="c", alpha=0.8)
    plt.xticks(x, xticks, size="small")
    plt.xlabel(name)
    plt.ylabel("Average Sync Delay")
    plt.title("Average Sync Delay - " + name)
    plt.show()
    # plot outInterest and outData
    plt.plot(x, out_interest, color="r", alpha=0.8, label="#(outInterest)")
    plt.plot(x, out_data, color="b", alpha=0.8, label="#(outData)")
    plt.xticks(x, xticks, size="small")
    plt.xlabel(name)
    plt.ylabel("Packet Number")
    plt.title("Packet Number - " + name)
    plt.legend(bbox_to_anchor=(0.2, 0.8), loc=2, borderaxespad=0.)
    plt.show()
  
def plot_statics():
  ave_degree = [0.08206876142284943, 0.31483480484515297, 0.6886629630015076, 1.1937351177658715, 1.820660418809224, 3.843645067144771, 6.450435820774718, 9.444968038269112]
  ave_partition_num = [38.3783803830538, 34.06969019556215, 27.906314058024588, 21.143619003018774, 14.893813726234733, 4.477356615703358, 1.545460172760808, 1.0898109118883266]
  ave_partition_degree = [0.9978202573463185, 0.990762274206064, 0.9760037268078395, 0.946357683908825, 0.8861889534250988, 0.44526418410272744, 0.061306388076070195, 0.007949117941246074]
  x = wifi_range
  # plot ave_degree
  plt.plot(x, ave_degree, alpha=0.8)
  plt.xlabel("Wifi Range (m)")
  plt.ylabel("Average Connection Degree")
  plt.title("Average Connection Degree - Wifi Range")
  plt.show()
  # plot ave_partition_num
  plt.plot(x, ave_partition_num, alpha=0.8)
  plt.xlabel("Wifi Range (m)")
  plt.ylabel("Average Partition Number")
  plt.title("Average Partition Number - Wifi Range")
  plt.show()
  # plot ave_partition_degree
  plt.plot(x, ave_partition_degree, alpha=0.8)
  plt.xlabel("Wifi Range (m)")
  plt.ylabel("Average Partition Degree")
  plt.title("Average Partition Degree - Wifi Range")
  plt.show()

if __name__ == "__main__":
  # get_cdf_pause()
  #get_cdf_loss()
  # get_ave_sync_duration()
  # get_cdf_range()
  # get_cdf_fast_resync()
  #get_cdf_heartbeat()
  #get_ave(heartbeat, "heartbeat", "Heartbeat Interval")
  # get_ave([True, False], "fastresync", "Fast Resync")
  # plot_statics()
  x = wifi_range
  x_label = "Wifi Range"
  beacon_state_delay, beacon_delay, beacon_collision, beacon_total_interest, beacon_notify_interest, beacon_data_interest, beacon_bundled_interest, beacon_total_data, beacon_data, beacon_bundled_data, beacon_beacon, beacon_ack = get_cdf("adhoc-result-new-pattern/syncDuration-range-beacon-30.txt", x, x_label)
  # heartbeat_state_delay, heartbeat_delay, heartbeat_collision, heartbeat_total_interest, heartbeat_notify_interest, heartbeat_data_interest, heartbeat_bundled_interest, heartbeat_total_data, heartbeat_data, heartbeat_bundled_data = get_cdf("adhoc-result-new-pattern/syncDuration-node-retx.txt", wifi_range, "Wifi Range")
  #beaconflood_state_delay, beaconflood_delay, beaconflood_collision, beaconflood_total_interest, beaconflood_notify_interest, beaconflood_data_interest, beaconflood_bundled_interest, beaconflood_total_data, beaconflood_data, beaconflood_bundled_data = get_cdf("adhoc-result2/syncDuration-range-retx-5.txt", wifi_range, "Wifi Range")
  retx_state_delay, retx_delay, retx_collision, retx_total_interest, retx_notify_interest, retx_data_interest, retx_bundled_interest, retx_total_data, retx_data, retx_bundled_data, retx_beacon, retx_ack = get_cdf("adhoc-result-new-pattern/syncDuration-range-retx-30.txt", x, x_label)

  plt.plot(x, beacon_state_delay, alpha=0.5, color='red', linewidth=2.0, label='Beacon')
  # plt.plot(x, heartbeat_state_delay, alpha=0.5, color='green', linewidth=2.0, label='Heartbeat')
  #plt.plot(x, beaconflood_state_delay, alpha=0.5, color='blue', linewidth=2.0, label='Retransmission Interval 5')
  plt.plot(x, retx_delay, alpha=0.5, color='blue', linewidth=2.0, label='Retransmission')
  plt.xlabel(x_label)
  plt.ylabel("State Sync Delay")
  plt.title("State Sync Delay - " + x_label)
  plt.legend(bbox_to_anchor=(0.4, 0.8), loc=2, borderaxespad=0.)
  plt.show()

  plt.plot(x, beacon_delay, alpha=0.5, color='red', linewidth=2.0, label='Beacon')
  # plt.plot(x, heartbeat_delay, alpha=0.5, color='green', linewidth=2.0, label='Heartbeat')
  #plt.plot(x, beaconflood_delay, alpha=0.5, color='blue', linewidth=2.0, label='Retransmission Interval 5')
  plt.plot(x, retx_delay, alpha=0.5, color='blue', linewidth=2.0, label='Retransmission')
  plt.xlabel(x_label)
  plt.ylabel("Data Sync Delay")
  plt.title("Data Sync Delay - " + x_label)
  plt.legend(bbox_to_anchor=(0.4, 0.8), loc=2, borderaxespad=0.)
  plt.show()

  plt.plot(x, beacon_collision, alpha=0.5, color='red', linewidth=2.0, label='Beacon')
  # plt.plot(x, heartbeat_collision, alpha=0.5, color='green', linewidth=2.0, label='Heartbeat')
  #plt.plot(x, beaconflood_collision, alpha=0.5, color='blue', linewidth=2.0, label='Retransmission Interval 5')
  plt.plot(x, retx_collision, alpha=0.5, color='blue', linewidth=2.0, label='Retransmission')
  plt.xlabel(x_label)
  plt.ylabel("Collision")
  plt.title("Collision - " + x_label)
  plt.legend(bbox_to_anchor=(0.1, 0.8), loc=2, borderaxespad=0.)
  plt.show()

  plt.plot(x, beacon_total_interest, alpha=0.5, color='red', linewidth=2.0, label='Beacon')
  # plt.plot(x, heartbeat_total_interest, alpha=0.5, color='green', linewidth=2.0, label='Heartbeat')
  #plt.plot(x, beaconflood_total_interest, alpha=0.5, color='blue', linewidth=2.0, label='Retransmission Interval 5')
  plt.plot(x, retx_total_interest, alpha=0.5, color='blue', linewidth=2.0, label='Retransmission')
  plt.xlabel(x_label)
  plt.ylabel("Out Total Interest")
  plt.title("Out Total Interest - " + x_label)
  plt.legend(bbox_to_anchor=(0.1, 0.8), loc=2, borderaxespad=0.)
  plt.show()

  plt.plot(x, beacon_notify_interest, alpha=0.5, color='red', linewidth=2.0, label='Beacon')
  # plt.plot(x, heartbeat_notify_interest, alpha=0.5, color='green', linewidth=2.0, label='Heartbeat')
  #plt.plot(x, beaconflood_notify_interest, alpha=0.5, color='blue', linewidth=2.0, label='Retransmission Interval 5')
  plt.plot(x, retx_notify_interest, alpha=0.5, color='blue', linewidth=2.0, label='Retransmission')
  plt.xlabel(x_label)
  plt.ylabel("Out Notify Interest")
  plt.title("Out Notify Interest - " + x_label)
  plt.legend(bbox_to_anchor=(0.1, 0.8), loc=2, borderaxespad=0.)
  plt.show()

  plt.plot(x, beacon_data_interest, alpha=0.5, color='red', linewidth=2.0, label='Beacon')
  # plt.plot(x, heartbeat_data_interest, alpha=0.5, color='green', linewidth=2.0, label='Heartbeat')
  #plt.plot(x, beaconflood_data_interest, alpha=0.5, color='blue', linewidth=2.0, label='Retransmission Interval 5')
  plt.plot(x, retx_data_interest, alpha=0.5, color='blue', linewidth=2.0, label='Retransmission')
  plt.xlabel(x_label)
  plt.ylabel("Out Data Interest")
  plt.title("Out Data Interest - " + x_label)
  plt.legend(bbox_to_anchor=(0.1, 0.8), loc=2, borderaxespad=0.)
  plt.show()

  plt.plot(x, beacon_bundled_interest, alpha=0.5, color='red', linewidth=2.0, label='Beacon')
  # plt.plot(x, heartbeat_bundled_interest, alpha=0.5, color='green', linewidth=2.0, label='Heartbeat')
  #plt.plot(x, beaconflood_bundled_interest, alpha=0.5, color='blue', linewidth=2.0, label='Retransmission Interval 5')
  plt.plot(x, retx_bundled_interest, alpha=0.5, color='blue', linewidth=2.0, label='Retransmission')
  plt.xlabel(x_label)
  plt.ylabel("Out Bundled Interest")
  plt.title("Out Bundled Interest - " + x_label)
  plt.legend(bbox_to_anchor=(0.1, 0.8), loc=2, borderaxespad=0.)
  plt.show()

  plt.plot(x, beacon_total_data, alpha=0.5, color='red', linewidth=2.0, label='Beacon')
  # plt.plot(x, heartbeat_total_data, alpha=0.5, color='green', linewidth=2.0, label='Heartbeat')
  #plt.plot(x, beaconflood_total_data, alpha=0.5, color='blue', linewidth=2.0, label='Retransmission Interval 5')
  plt.plot(x, retx_total_data, alpha=0.5, color='blue', linewidth=2.0, label='Retransmission')
  plt.xlabel(x_label)
  plt.ylabel("Out Total Data")
  plt.title("Out Total Data - " + x_label)
  plt.legend(bbox_to_anchor=(0.1, 0.8), loc=2, borderaxespad=0.)
  plt.show()

  plt.plot(x, beacon_data, alpha=0.5, color='red', linewidth=2.0, label='Beacon')
  # plt.plot(x, heartbeat_data, alpha=0.5, color='green', linewidth=2.0, label='Heartbeat')
  #plt.plot(x, beaconflood_data, alpha=0.5, color='blue', linewidth=2.0, label='Retransmission Interval 5')
  plt.plot(x, retx_data, alpha=0.5, color='blue', linewidth=2.0, label='Retransmission')
  plt.xlabel(x_label)
  plt.ylabel("Out Data")
  plt.title("Out Data - " + x_label)
  plt.legend(bbox_to_anchor=(0.1, 0.8), loc=2, borderaxespad=0.)
  plt.show()

  plt.plot(x, beacon_bundled_data, alpha=0.5, color='red', linewidth=2.0, label='Beacon')
  # plt.plot(x, heartbeat_bundled_data, alpha=0.5, color='green', linewidth=2.0, label='Heartbeat')
  #plt.plot(x, beaconflood_bundled_data, alpha=0.5, color='blue', linewidth=2.0, label='Retransmission Interval 5')
  plt.plot(x, retx_bundled_data, alpha=0.5, color='blue', linewidth=2.0, label='Retransmission')
  plt.xlabel(x_label)
  plt.ylabel("Out Bundled Data")
  plt.title("Out Bundled Data - " + x_label)
  plt.legend(bbox_to_anchor=(0.1, 0.8), loc=2, borderaxespad=0.)
  plt.show()
