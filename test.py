import numpy as np
from interval import interval, inf, imath
import re
import os
import os.path
  
t1 = [384623.0, 388679.0, 392542.0, 396793.0, 396743.0, 396790.0, 384534.0, 384858.0, 384825.0, 384759.0]
t2 = [623.0, 679.0, 542.0, 793.0, 743.0, 790.0, 534.0, 858.0, 825.0, 759.0]
ans = []
for i in range(10):
	ans.append(t2[i] / t1[i])
print ans