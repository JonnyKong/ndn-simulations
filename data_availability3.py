import numpy as np
from interval import interval, inf, imath
import re

file = open("snapshot.txt")
pattern = re.compile(r'\(([0-9]+)\:([0-9]+)-([0-9]+)\)')
pattern2 = re.compile(r'\(([0-9]+)\:([0-9]+)\)')
node_num = 10
snapshot_num = 25

data_snapshots_array = []
rw_snapshots_array = []
vv_snapshots_array = []
data_name = []

line_idx = -1
node_id = -1
for line in file:
    line_idx += 1
    if line_idx % (2 * snapshot_num + 1) == 0:
        new_data_snapshot = map(int, line.split(",")[:-1])
        data_snapshots_array.append(new_data_snapshot)
        node_id += 1
    elif line_idx % (2 * snapshot_num + 1) >= 1 and line_idx % (2 * snapshot_num + 1) <= snapshot_num:
        time_index = line_idx % (2 * snapshot_num + 1) - 1
        if len(rw_snapshots_array) == time_index:
            dict = {}
            rw_snapshots_array.append(dict)
        data_dict = {}
        for (data_nodeID, startSeq, endSeq) in re.findall(pattern, line):
            for i in range(int(startSeq), int(endSeq) + 1):
                data_dict[str(data_nodeID) + "-" + str(i)] = 1
        rw_snapshots_array[time_index][node_id] = data_dict
        '''
        interval_dict = {}
        for (data_nodeID, startSeq, endSeq) in re.findall(pattern, line):
          data_nodeID = int(data_nodeID)
          if data_nodeID in interval_dict:
            interval_dict[data_nodeID] |= interval[startSeq, endSeq]
          else:
            interval_dict[data_nodeID] = interval[startSeq, endSeq]
        rw_snapshots_array[time_index][node_id] = interval_dict
        '''
    else:
        if line == "":
            continue
        time_index = line_idx % (2 * snapshot_num + 1) - snapshot_num - 1
        if len(vv_snapshots_array) == time_index:
            dict = {}
            vv_snapshots_array.append(dict)
        vv_dict = {}
        for (seq_nodeID, seq) in re.findall(pattern, line):
            seq_nodeID = int(seq_nodeID)
            vv_dict[seq_nodeID] = int(seq)
        vv_snapshots_array[time_index][node_id] = vv_dict

data_snapshots = np.array(data_snapshots_array)
merged_data_snapshots = np.zeros((snapshot_num, node_num))
for i in range(snapshot_num):
    for node in range(node_num):
        merged_data_snapshots[i, node] = data_snapshots[node, i]

for node in range(node_num):
    max_seq = merged_data_snapshots[snapshot_num - 1][node]
    for i in range(int(max_seq)):
        data_name.append(str(node) + "-" + str(i + 1))
print("data name:")
print(data_name)

data_trace = {}
for data in data_name:
    data_trace[data] = np.zeros(snapshot_num)
for data in data_name:
    for i in range(snapshot_num):
        cover_node = 0
        for nid in range(node_num):
            if data in rw_snapshots_array[i][nid]:
                cover_node += 1
        data_trace[data][i] = cover_node
print(data_trace)


