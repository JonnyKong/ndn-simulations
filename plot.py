import os
import os.path
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import math

five_node = np.array([1113, 1118, 1119, 1119, 1120])
six_node = np.array([1225, 1227, 1230, 1231, 1217, 1225])
seven_node = np.array([1310, 1312, 1309, 1312, 1321, 1317, 1325])
eight_node = np.array([1270, 1269, 1265, 1270, 1271, 1280, 1266, 1268])
nine_node = np.array([1283, 1280, 1280, 1281, 1265, 1264, 1276, 1276, 1276])
ten_node = np.array([1318, 1338, 1333, 1346, 1337, 1348, 1330, 1326, 1337, 1333])
eleven_node = np.array([1350, 1346, 1358, 1358, 1362, 1358, 1374, 1369, 1374, 1369, 1350])
twelve_node = np.array([1374, 1378, 1382, 1386, 1374, 1374, 1386, 1394, 1398, 1406, 1401, 1393])
thirteen_node = np.array([1345, 1349, 1345, 1361, 1357, 1361, 1346, 1330, 1342, 1350, 1338, 1345, 1353])
fourteen_node = np.array([1640, 1640, 1644, 1648, 1648, 1652, 1652, 1660, 1660, 1621, 1633, 1633, 1632, 1649])
fifteen_node = np.array([1511, 1519, 1515, 1519, 1523, 1523, 1527, 1534, 1539, 1531, 1543, 1547, 1547, 1554, 1550])
node_num_to_data = {5: five_node, 6: six_node, 7: seven_node, 8: eight_node, 9: nine_node, 10: ten_node, 11: eleven_node, 12: twelve_node, 13: thirteen_node, 14: fourteen_node, 15: fifteen_node}

data_availability = np.array([1.0, 1.0, 1.0, 1.0, 1.0,1.0, 1.0, 1.0, 1.0, 1.0, 1.0])
node_num = np.array([5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15])

def plot_availability():
  plt.plot(node_num, data_availability)
  plt.title('Data Availability - #(Node in Group)')
  plt.xlabel("The number of nodes in group")
  plt.ylabel("Data Availability")
  plt.ylim((0.97, 1.03))
  plt.show()

def plot_sleeping_time_distribution(node_num):
  y = node_num_to_data[node_num]
  mean = np.mean(y)
  std = np.std(y)
  plt.scatter(range(node_num), y)
  plt.title("Sleeping Time Distribution for " + str(node_num) + "-node Group")
  plt.xlabel("Node Index")
  plt.ylabel("Sleeping Time (seconds)")
  plt.ylim((800, 1800))
  plt.show()
  print mean, std

def plot_sleeping_node_num():
  ave = np.array([1117.8, 1225.83333333, 1315.14285714, 1269.875, 1275.66666667, 1334.6, 1360.72727273, 1387.16666667, 1347.84615385, 1643.71428571, 1532.13333333 ])
  std = np.array([2.4819347292, 4.5613107278, 5.59153879737, 4.28478412525, 6.41179468722, 8.416650165, 9.34287194595, 10.7147354403, 8.50200115671, 10.8326792058, 13.3858963922])

  plt.errorbar(node_num, ave, std, linestyle='None', marker='^')
  plt.title("Average Sleeping Time - #(Node in Group)")
  plt.xlabel("The number of nodes in group")
  plt.ylabel("Avarage Sleeping Time")
  plt.show()


if __name__ == "__main__":
  # plot_availability()
  '''
  for node_num in node_num_to_data.keys():
    plot_sleeping_time_distribution(node_num)
  '''
  plot_sleeping_node_num()
  