# compute mean, variance, and total for a series of task times produced by extractTimes.bash
#

import fileinput
import math

sum = 0
numValues = 0
values = []

for line in fileinput.input():
  words = line.split(' ')
  taskName = words[11]
  if not taskName.startswith('<main_task'):
    elapsedNS = long(words[12])
    values.append(elapsedNS)
    sum = sum + elapsedNS
    numValues = numValues + 1

if numValues == 0:
  values.append(0)
  numValues = 1

mean = sum / numValues
sumVariance = 0

for i in range(numValues):
  difference = values[i] - mean
  sumVariance = sumVariance + difference * difference

variance = sumVariance / numValues
if numValues == 1:
  numValues = 2 # just for std dev
standardDeviation = math.sqrt(sumVariance / (numValues - 1))

print "totalTime", sum, "taskMean", mean, "taskVariance", variance, "taskStdDeviation", standardDeviation, "numValues", numValues
