import numpy as np
from interval import interval, inf, imath
import re
import os
import os.path
import matplotlib.pyplot as plt
import pylab as pl
import sys

test = sys.argv[1]
print(test)
with open("test-xin.txt", "r") as f:
	for line in f:
		print(line[1:-2])
		str = "[0,1]"
		print(str[1:-1])
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
