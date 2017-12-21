import os
import os.path
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import math

five_node = np.array([90, 92, 88, 94, 98])
six_node = np.array([114, 118, 108, 108, 102, 108])
seven_node = np.array([124, 106, 118, 118, 118, 120, 120])
eight_node = np.array([138, 126, 108, 116, 112, 112, 130, 126])
nine_node = np.array([128, 128, 128, 128, 124, 132, 136, 128, 128])
ten_node = np.array([102, 110, 106, 106, 114, 114, 152, 152, 140, 148])
eleven_node = np.array([114, 122, 114, 122, 126, 126, 168, 168, 126, 118, 118])
twelve_node = np.array([130, 130, 92, 92, 138, 138, 138, 138, 138, 138, 126, 134])
thirteen_node = np.array([88, 96, 100, 100, 150, 150, 150, 150, 150, 150, 150, 138, 146])
fourteen_node = np.array([96, 104, 108, 108, 162, 162, 162, 162, 162, 162, 108, 108, 96, 104])
fifteen_node = np.array([108, 108, 116, 116, 174, 174, 116, 116, 116, 116, 116, 116, 116, 104, 112])
node_num_to_data = {5: five_node, 6: six_node, 7: seven_node, 8: eight_node, 9: nine_node, 10: ten_node, 11: eleven_node, 12: twelve_node, 13: thirteen_node, 14: fourteen_node, 15: fifteen_node}

data_availability = np.array([0.963076923077, 0.994871794872, 0.962637362637, 0.998076923077, 0.986324786325, 0.970769230769, 1.0, 1.0, 1.0, 0.984615384615, 0.982564102564])
node_num = np.array([5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15])

def plot_availability():
  plt.plot(node_num, data_availability)
  plt.title('Data Availability - #(Node in Group)')
  plt.xlabel("The number of nodes in group")
  plt.ylabel("Data Availability")
  plt.ylim((0.9, 1.1))
  plt.show()

def plot_sleeping_time_distribution(node_num):
  y = node_num_to_data[node_num]
  mean = np.mean(y)
  std = np.std(y)
  plt.scatter(range(node_num), y)
  plt.title("Sleeping Time Distribution for " + str(node_num) + "-node Group")
  plt.xlabel("Node Index")
  plt.ylabel("Sleeping Time (seconds)")
  plt.ylim((0, 200))
  plt.show()
  print mean, std

def plot_sleeping_node_num():
  ave = np.array([92.4, 109.666666667, 117.714285714, 121.0, 128.888888889, 124.4, 129.272727273, 127.666666667, 132.153846154, 128.857142857, 121.6])
  std = np.array([3.44093010682, 5.08811250749, 5.17450579322, 9.8488578018, 3.14269680527, 19.8151457224, 18.728155319, 16.4282953738, 24.4629897973, 28.9404737419, 20.8895508169])

  plt.errorbar(node_num, ave, std, linestyle='None', marker='^')
  plt.title("Average Sleeping Time - #(Node in Group)")
  plt.xlabel("The number of nodes in group")
  plt.ylabel("Avarage Sleeping Time")
  plt.show()


if __name__ == "__main__":
  #plot_availability()
  '''
  for node_num in node_num_to_data.keys():
    plot_sleeping_time_distribution(node_num)
  '''
  plot_sleeping_node_num()
  