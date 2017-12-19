import os
import os.path
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import math

five_node = np.array([95, 94, 104, 108, 109])
six_node = np.array([114, 115, 103, 107, 108, 112])
seven_node = np.array([127, 112, 119, 112, 120, 120, 127])
eight_node = np.array([132, 143, 109, 117, 113, 113, 132, 117])
ten_node = np.array([107, 107, 107, 107, 114, 114, 152, 152, 145, 145])
twelve_node = np.array([127, 135, 92, 92, 138, 138, 138, 138, 138, 138, 131, 131])
fifteen_node = np.array([116, 113, 116, 116, 174, 174, 116, 73, 116, 116, 116, 116, 116, 105, 113])
node_num_to_data = {5: five_node, 6: six_node, 7: seven_node, 8: eight_node, 10: ten_node, 12: twelve_node, 15: fifteen_node}

data_availability = np.array([0.84, 0.87, 0.83, 0.81, 0.8, 0.85, 0.77])
node_num = np.array([5, 6, 7, 8, 10, 12, 15])

def plot_availability():
  plt.plot(node_num, data_availability)
  plt.title('Data Availability - #(Node in Group)')
  plt.xlabel("The number of nodes in group")
  plt.ylabel("Data Availability")
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
  ave = np.array([102.0, 109.833333333, 119.571428571, 122.0, 125.0, 128.0, 119.733333333])
  std = np.array([6.35609943283, 4.21966296706, 5.67845911839, 11.3026545555, 19.4833262047, 16.4823137534, 23.8144680581])

  plt.errorbar(node_num, ave, std, linestyle='None', marker='^')
  plt.title("Average Sleeping Time - #(Node in Group)")
  plt.xlabel("The number of nodes in group")
  plt.ylabel("Avarage Sleeping Time")
  plt.show()


if __name__ == "__main__":
  plot_availability()
  '''
  for node_num in node_num_to_data.keys():
    plot_sleeping_time_distribution(node_num)
  '''
  #plot_sleeping_node_num()
  