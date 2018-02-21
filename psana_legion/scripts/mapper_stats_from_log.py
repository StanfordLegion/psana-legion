#!/usr/bin/python
#
# Compile statistics from the report_profiling lines of a mapper log file.
# Keep separate statistics for each processor type.
# Compute count, total, mean and standard deviation of task weights.
# Compute effective load balance.  Predict random load balance.
#
#



import fileinput
import math
import random

statistics = {}
statistics["top"] = { "durations": [], "totalDuration": 0, "numTasks": 0, "mean": 0, "standardDeviation": 0, "randomTotalDuration": 0 }
balance = {}
mapper = None

print "Reading logs ..."

nodesToSkip = [ '1d0000' ]
for line in fileinput.input():
  words = line.split(' ')
  duration = None
  if len(words) == 20:
    if words[10].startswith('report_profiling'):
      mapper = words[3]
      node = words[6]
      proc = words[8]
      task = words[14]
      duration = long(words[15])
  elif len(words) == 19:
    if words[9].startswith('report_profiling'):
      mapper = words[3]
      node = words[6]
      proc = words[8]
      task = words[13]
      duration = long(words[14])

  if duration == None:
    continue
  if node in nodesToSkip:
    continue

  procType = proc.split('(')[1][:-2]
  if procType not in balance:
    balance[procType] = { "imbalance": 0, "randomImbalance": 0, "numProcs": 0, "durations": [] }
  balance[procType]["durations"].append(duration)
      
  key = node + ':' + proc

  for k in [ key, procType, "top" ]:
    if k not in statistics:
      statistics[k] = { "durations": [], "totalDuration": 0, "numTasks": 0, "mean": 0, "standardDeviation": 0, "randomTotalDuration": 0 }
    statistics[k]["durations"].append(duration)
    statistics[k]["totalDuration"] = statistics[k]["totalDuration"] + duration
    statistics[k]["numTasks"] = statistics[k]["numTasks"] + 1

print "Crunching data ..."

for key in statistics:
  words = key.split(':')
  if len(words) == 3:
    proc = words[1]
    procType = proc.split('(')[1][:-1]
    balance[procType]["numProcs"] = balance[procType]["numProcs"] + 1
  numTasks = statistics[key]["numTasks"]
  if numTasks == 0:
    numTasks = 1
  statistics[key]["mean"] = statistics[key]["totalDuration"] / numTasks
  if statistics[key]["numTasks"] > 1:
    sum = 0
    for duration in statistics[key]["durations"]:
      delta = duration - statistics[key]["mean"]
      sum = sum + delta * delta
    standardDeviation = math.sqrt(sum / (statistics[key]["numTasks"] - 1))
    statistics[key]["standardDeviation"] = standardDeviation

print "Computing random assignment ..."

numKeys = len(statistics.items())
for key in balance:
  for duration in balance[key]["durations"]:
    foundIt = False
    while True:
      index = random.randint(0, numKeys - 1)
      statsKey = statistics.items()[index][0]
      if statsKey == 'top':
        continue
      if statsKey.split(':')[0] in nodesToSkip:
        continue
      keyWords = statsKey.split('(')
      if len(keyWords) == 1:
        continue
      procType = keyWords[1][:-2]
      if procType == key:
        foundIt = True
        break
    if foundIt == True:
      statistics[statsKey]["randomTotalDuration"] = statistics[statsKey]["randomTotalDuration"] + duration

print "Computing balance ..."

for key in balance:
  for statsKey in statistics:
    words = statsKey.split(':')
    if len(words) == 3:
      proc = words[1]
      statsProcType = proc.split('(')[1][:-1]
      if statsProcType == key:
        numProcs = balance[key]["numProcs"]
        idealTime = float(statistics[key]["totalDuration"]) / numProcs
        imbalance = float(statistics[statsKey]["totalDuration"]) / idealTime
        balance[key]["imbalance"] = max(balance[key]["imbalance"], imbalance)
        randomImbalance = float(statistics[statsKey]["randomTotalDuration"]) / idealTime
        balance[key]["randomImbalance"] = max(balance[key]["randomImbalance"], randomImbalance)

print "mapper", mapper
for key in sorted(statistics):
  print key, "numTasks", statistics[key]["numTasks"], "totalDuration", statistics[key]["totalDuration"], "mean", statistics[key]["mean"], "standardDeviation", statistics[key]["standardDeviation"], "randomTotalDuration", statistics[key]["randomTotalDuration"]

print "key", "numProcs", "imbalance", "randomImbalance"
for key in balance:
  print key, balance[key]["numProcs"], balance[key]["imbalance"], balance[key]["randomImbalance"]


