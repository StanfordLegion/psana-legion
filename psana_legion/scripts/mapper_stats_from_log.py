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
statistics["top"] = { "durations": [], "totalDuration": 0, "numTasks": 0, "mean": 0, "standardDeviation": 0 }
balance = {}
mapper = None
durations = []

for line in fileinput.input():
  words = line.split(' ')
  if len(words) == 20:
    if words[10].startswith('report_profiling'):
      
      mapper = words[3]
      node = words[6]
      proc = words[8]
      task = words[14]
      duration = long(words[15])
      
      procType = proc.split('(')[1][:-2]
      if procType not in balance:
        balance[procType] = { "balance": 0, "randomBalance": 0, "numProcs": 0 }
      key = node + ':' + proc

      for k in [ key, procType, "top" ]:
        if k not in statistics:
          statistics[k] = { "durations": [], "totalDuration": 0, "numTasks": 0, "mean": 0, "standardDeviation": 0, "randomTotalDuration": 0 }
        durations.append(duration)
        statistics[k]["durations"].append(duration)
        statistics[k]["totalDuration"] = statistics[k]["totalDuration"] + duration
        statistics[k]["numTasks"] = statistics[k]["numTasks"] + 1


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

numKeys = len(statistics.items)
for duration in durations:
  index = random.randint(0, numKeys - 1)
  key = statistics.items()[0][0]
  statistics[key]["randomTotalDuration"] = statistics[key]["randomTotalDuration"] + duration

for key in balance:
  for statsKey in statistics:
    words = statsKey.split(':')
    if len(words) == 3:
      proc = words[1]
      statsProcType = proc.split('(')[1][:-1]
      if statsProcType == key:
        meanTime = float(statistics[key]["totalDuration"]) / balance[key]["numProcs"]
        runtime = float(statistics[statsKey]["totalDuration"]) / meanTime
        balance[key]["balance"] = max(balance[key]["balance"], runtime)
        randomRuntime = float(statistics[statsKey]["randomTotalDuration"]) / meanTime
        balance[key]["randomBalance"] = max(balance[key]["randomBalance"], randomRuntime)

print "mapper", mapper
for key in sorted(statistics):
  print key, "numTasks", statistics[key]["numTasks"], "totalDuration", statistics[key]["totalDuration"], "mean", statistics[key]["mean"], "standardDeviation", statistics[key]["standardDeviation"]

for key in balance:
  print key, "balance", balance[key]


