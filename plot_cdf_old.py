import json
import matplotlib.pyplot as plt

cdf_file="cdf.txt"
colors = ["b", "g", "r", "c", "m", "y"]
lines = [None for i in range(6)]

with open(cdf_file, "r") as f:
    i = 0
    for line in f.readlines():
        (bin_edges, cdf_result) = json.loads(line.strip())
        lines[i], = plt.plot(bin_edges[0:-1], cdf_result, linestyle='-', color=colors[i])
        i += 1

plt.legend(lines, [i for i in range(60, 161, 20)])

plt.title("Percentage of synced state over time")
plt.xlabel("Time (s)")
plt.ylim((0,1))
plt.ylabel("CDF")
plt.grid(True)
plt.show()