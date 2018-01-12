import numpy as np
from interval import interval, inf, imath
import re

file = open("snapshot.txt")
pattern = re.compile(r'\(([0-9]+)\:([0-9]+)-([0-9]+)\)')
node_num = 10
snapshot_num = 150
sleeping_time = []
#first_syncACK_delay = []
#last_syncACK_delay = []
suppression = []
outInterest = []
collision_num = []
data_snapshots_array = []
active_snapshots_array = []
rw_snapshots_array = []

line_idx = 0
node_id = -1
for line in file:
  if line_idx % (snapshot_num + 5) == 0:
    sleeping_time.append(int(line))
    node_id += 1
  elif line_idx % (snapshot_num + 5) == 1:
    suppression.append(float(line))
  elif line_idx % (snapshot_num + 5) == 2:
    outInterest.append(float(line))
  elif line_idx % (snapshot_num + 5) == 3:
    new_active_snapshot = map(int, line.split(","))
    active_snapshots_array.append(new_active_snapshot)
  elif line_idx % (snapshot_num + 5) == 4:
    new_data_snapshot = map(int, line.split(","))
    data_snapshots_array.append(new_data_snapshot)

    node_snapshot_list = []
    rw_snapshots_array.append(node_snapshot_list)
  else:
    recv_window = {}
    for (nodeID, startSeq, endSeq) in re.findall(pattern, line):
      nodeID = int(nodeID)
      if nodeID in rw_snapshots_array[node_id]:
        recv_window[nodeID] |= interval[startSeq, endSeq]
      else:
        recv_window[nodeID] = interval[startSeq, endSeq]
    rw_snapshots_array[node_id].append(recv_window)
  line_idx += 1
# convert to np array
data_snapshots = np.array(data_snapshots_array)
active_snapshots = np.array(active_snapshots_array)

# check the correctness of the above structures
'''
print(energy_consumption)
print("\n")
print(data_snapshots)
print("\n")
for node in vv_snapshots:
  for line in node:
    print(line)
  print("\n")
'''

# get the true latest data snapshot
merged_data_snapshots = np.zeros((snapshot_num, node_num))
for i in range(snapshot_num):
  for node in range(node_num):
    merged_data_snapshots[i, node] = data_snapshots[node, i]

active_num = np.zeros(snapshot_num)
for i in range(snapshot_num):
  for j in range(node_num):
    active_num[i] += active_snapshots[j, i]

merged_rw_snapshots = rw_snapshots_array[0]
for node_snapshot_list in rw_snapshots_array:
  for i in range(snapshot_num):
    for nodeID, nodeInterval in node_snapshot_list[i].items():
      if nodeID in merged_rw_snapshots[i]:
        merged_rw_snapshots[i][nodeID] |= node_snapshot_list[i][nodeID]
      else:
        merged_rw_snapshots[i][nodeID] = node_snapshot_list[i][nodeID]
'''
for i in range(snapshot_num):
  print merged_rw_snapshots[i]
'''

hitting_data_count = 0.0
total_data_count = 0.0
for i in range(snapshot_num):
  for nodeID in range(node_num):
    if merged_data_snapshots[i, nodeID] != 0:
      if nodeID not in merged_rw_snapshots[i]:
        total_data_count += merged_data_snapshots[i, nodeID]
      else:
        data_interval = merged_rw_snapshots[i][nodeID]
        for seq in range(1, int(merged_data_snapshots[i, nodeID]) + 1):
          total_data_count += 1
          if seq in data_interval:
            hitting_data_count += 1

print((hitting_data_count / total_data_count))
print(np.sum(np.array(suppression)))
print(np.sum(np.array(outInterest)))
print(sleeping_time)
print(np.mean(active_num))



  





