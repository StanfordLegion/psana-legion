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
mu = 196385139
sigma = 4592620946
epsilon = 0.5

first = True
n = 1
for i in range(10):
  m = 1024
  line = []
  if first:
    print "    "
    header = []
  for j in range(10):
    if first:
      header.append(m)
    pr = 0.5 + 0.5 * math.erf(epsilon * mu * m / (n * sigma * math.sqrt(2 * m / n)))
    prp = math.pow(pr, n)
    line.append(prp)
    m = m * 2
  n = n * 2
  if first:
    print header
  print "n=", n, line
  first = False
