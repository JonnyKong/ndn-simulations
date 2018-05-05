import numpy as np
from interval import interval, inf, imath
import re
import os
import os.path
import matplotlib.pyplot as plt
import pylab as pl
import sys

a = [1, 2, 3]
b = [1, 2, 3]
c = [i + j for i, j in zip(a, b)]
print(c)
'''
x = []
y = []
file = open("test.txt")
for line in file:
	elements = line.split(' ')
	cur_x = float(elements[3])
	cur_y = float(elements[4])
	x.append(cur_x)
	y.append(cur_y)

#fig = pyplot.figure()
#ax = fig.add_subplot(111)
#ax.set_ylim(0,10)
plt.scatter(x,y)
for nid in range(20):
    plt.annotate(str(nid),xy=(x[nid], y[nid]))

plt.show()
'''

'''
test = {}
test[1] = 1
test[2] = 2
test[3] = 3
for key, value in test.iteritems():
	print value
d = float(sum(test.values())) / len(test)
print d
'''
