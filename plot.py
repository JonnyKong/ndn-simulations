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
wifi_range = [250, 200, 150, 100, 80, 60, 40, 20]
heartbeat = [3, 5, 10, 15, 20, 25, 30, 40]
log_line = 12

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

def get_cdf_range():
  file_name = "adhoc-result/syncDuration-range-onetime.txt"
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
      cdf_plot(sync_delay, "wifi range = " + str(wifi_range[idx / log_line]), 1000, color[idx / log_line])
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
  plt.title("CDF of sync delay of different wifi ranges")
  plt.ylim(0)
  plt.xlim(-50)
  plt.legend(bbox_to_anchor=(0.60, 0.55), loc=2, borderaxespad=0.)
  plt.show()

  xticks = ['250', '200', '150', '100', '80', '60', '40', '20']
  plt.bar(np.arange(len(wifi_range)) + 1, data_availability_list, width=0.35, align="center", color="c", alpha=0.8)
  plt.xticks(np.arange(len(wifi_range)) + 1, xticks, size="small")
  plt.ylim(0.0, 1.2)
  plt.xlabel("Wifi Range")
  plt.ylabel("Data Availability")
  plt.title("Data Availability - Wifi Range")
  plt.show()

  plt.errorbar(wifi_range, delay, std, linestyle='None', marker='^')
  plt.plot(wifi_range, delay, alpha=0.5, color='b')
  plt.xlabel("Wifi Range")
  plt.ylabel("Sync Delay")
  plt.title("Sync Delay - Wifi Range")
  plt.xlim(15, 255)
  plt.show()

  plt.errorbar(wifi_range, out_interest, std_out_interest, linestyle='None', marker='^', color='b')
  plt.plot(wifi_range, out_interest, alpha=0.5, color='b', label="#(outInterest)")
  plt.errorbar(wifi_range, out_data, std_out_data, linestyle='None', marker='^', color='r')
  plt.plot(wifi_range, out_data, alpha=0.5, color='r', label="#(outData)")
  plt.xlabel("Wifi Range")
  plt.ylabel("Packet Number")
  plt.xlim(15, 255)
  plt.title("Packet Number - Wifi Range")
  plt.legend(bbox_to_anchor=(0.60, 0.55), loc=2, borderaxespad=0.)
  plt.show()  


def get_cdf_loss():
  file_name = "adhoc-result/syncDuration-loss-onetime.txt"
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
      cdf_plot(sync_delay, "loss rate = " + str(loss[idx / log_line]), 1000, color[idx / log_line])
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
  plt.title("CDF of sync delay of different loss rate")
  plt.ylim(0)
  plt.xlim(-50)
  plt.legend(bbox_to_anchor=(0.55, 0.45), loc=2, borderaxespad=0.)
  plt.show()

  xticks = ['0.0', '0.01', '0.05', '0.1', '0.3', '0.5']
  plt.bar(np.arange(len(loss)) + 1, data_availability_list, width=0.35, align="center", color="c", alpha=0.8)
  plt.xticks(np.arange(len(loss)) + 1, xticks, size="small")
  plt.ylim(0.0, 1.2)
  plt.xlabel("Loss Rate")
  plt.ylabel("Data Availability")
  plt.title("Data Availability - Loss Rate")
  plt.show()

  #plt.errorbar(loss, delay, std, linestyle='None', marker='^')
  plt.plot(loss, delay, alpha=0.5, color='b')
  plt.xlabel("Loss Rate")
  plt.ylabel("Sync Delay")
  plt.title("Sync Delay - Loss Rate")
  plt.xlim(-0.1, 0.6)
  plt.show()

  plt.errorbar(loss, out_interest, std_out_interest, linestyle='None', marker='^', color='b')
  plt.plot(loss, out_interest, alpha=0.5, color='b', label="#(outInterest)")
  plt.errorbar(loss, out_data, std_out_data, linestyle='None', marker='^', color='r')
  plt.plot(loss, out_data, alpha=0.5, color='r', label="#(outData)")
  plt.xlabel("Loss Rate")
  plt.ylabel("Packet Number")
  plt.xlim(-0.1, 0.6)
  plt.title("Packet Number - Loss Rate")
  plt.legend(bbox_to_anchor=(0.2, 0.8), loc=2, borderaxespad=0.)
  plt.show()  

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


def get_cdf_pause():
  file_name = "adhoc-result/syncDuration-pause-onetime.txt"
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
      cdf_plot(sync_delay, "pause time = " + str(pause[idx / log_line]), 1000, color[idx / log_line])
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
  plt.title("CDF of sync delay of different pause time")
  plt.ylim(0)
  plt.xlim(-50)
  plt.legend(bbox_to_anchor=(0.55, 0.45), loc=2, borderaxespad=0.)
  plt.show()

  xticks = ["0", "30", "60", "120", "300", "600", "900"]
  plt.bar(np.arange(len(pause)) + 1, data_availability_list, width=0.35, align="center", color="c", alpha=0.8)
  plt.xticks(np.arange(len(pause)) + 1, xticks, size="small")
  plt.ylim(0.0, 1.2)
  plt.xlabel("Pause Time")
  plt.ylabel("Data Availability")
  plt.title("Data Availability - Pause Time")
  plt.show()

  # plt.errorbar(loss, delay, std, linestyle='None', marker='^')
  plt.plot(pause, delay, alpha=0.5, color='b')
  plt.xlabel("Pause Time")
  plt.ylabel("Sync Delay")
  plt.xlim(-10, 910)
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
  

if __name__ == "__main__":
  # get_cdf_pause()
  #get_cdf_loss()
  # get_ave_sync_duration()
  # get_cdf_range()
  # get_cdf_fast_resync()
  get_cdf_heartbeat()
  get_ave(heartbeat, "heartbeat", "Heartbeat Interval")
  # get_ave([True, False], "fastresync", "Fast Resync")