import numpy as np
from interval import interval, inf, imath
import re
import os
import os.path
import matplotlib.pyplot as plt
import pylab as pl
  
xs = []
ys = []
file = open("test.txt")
for line in file:
    data = line.split(" ")
    #print(data)
    cur_x = float(data[3])
    cur_y = float(data[4])
    xs.append(cur_x)
    ys.append(cur_y)
plt.scatter(xs, ys)
for i in range(20):
    x = xs[i]
    y = ys[i]
    pl.text(x, y, str(i), color="red", fontsize=12)
plt.show()