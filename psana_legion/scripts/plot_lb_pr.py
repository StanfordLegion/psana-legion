#!/usr/bin/python
#
# Pr = 1/2 + 1/2 Erf( epsilon mu m / (n sigma sqrt(2 m / n)))
# epsilon = desired load imbalance eg .5 for 50% imbalance
# mu = mean eg 196385139
# sigma = standard deviation eg 4592620946
# m is number of tasks, sweep
# n is number of processors, sweep

import math

# change this
mu = 174377619
sigma = 171976504
epsilon = 0.2
m = 85430

first = True
n = 32
for i in range(64):
  pr = 0.5 + 0.5 * math.erf(epsilon * mu * m / (n * sigma * math.sqrt(2 * m / n)))
  prp = math.pow(pr, n)
  print n, prp
  n = n + 32

print ""
print ""

n = 32
for i in range(64):
  pr = 0.5 + 0.5 * math.erf(epsilon * mu * m / (n * sigma * math.sqrt(2 * m / n)))
  prp = math.pow(pr, n)
  print n, pr
  n = n + 32

