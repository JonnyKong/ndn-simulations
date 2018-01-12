import numpy as np
from interval import interval, inf, imath
import re
import os
import os.path

file = open("test.txt")
total = 0.0
for line in file:
	total += float(line)
print(760.0 / total)