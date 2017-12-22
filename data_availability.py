import numpy as np

file = open("snapshot.txt")
node_num = 10
snapshot_num = 65
sleeping_time = []
data_snapshots_array = []
vv_snapshots_array = []

line_idx = 0
node_id = -1
for line in file:
  if line_idx % (snapshot_num + 2) == 0:
    sleeping_time.append(int(line))
    node_id += 1
  elif line_idx % (snapshot_num + 2) == 1:
    new_data_snapshot = map(int, line.split(","))
    data_snapshots_array.append(new_data_snapshot)
  elif line_idx % (snapshot_num + 2) == 2:
    new_vv_snapshot = []
    new_vv_snapshot.append(map(int, line.split(",")))
    vv_snapshots_array.append(new_vv_snapshot)
  else:
    vv_snapshots_array[node_id].append(map(int, line.split(",")))
  line_idx += 1
# convert to np array
data_snapshots = np.array(data_snapshots_array)
vv_snapshots = np.array(vv_snapshots_array)

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

merged_vv_snapshots = np.zeros((snapshot_num, node_num))
for i in range(snapshot_num):
  for node in range(node_num):
    if node == 0:
      merged_vv_snapshots[i, :] = vv_snapshots[node, i, :]
    else:
      cur_vv = vv_snapshots[node, i, :]
      for index in range(node_num):
        if merged_vv_snapshots[i, index] < cur_vv[index]:
            merged_vv_snapshots[i, index] = cur_vv[index]


for item in merged_data_snapshots:
  print(item)
print("\n")

for item in merged_vv_snapshots:
  print(item)
print("\n")


total = 0.0
success_num = 0.0
for i in range(snapshot_num):
  success = True
  for j in range(node_num):
    total += 1
    if merged_vv_snapshots[i, j] == merged_data_snapshots[i, j]:
      success_num += 1
print "data availability = ", (success_num / total)

print(sleeping_time)


  





