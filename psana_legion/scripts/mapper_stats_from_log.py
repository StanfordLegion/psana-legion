#!/usr/bin/python
#
# Compile statistics from the report_profiling lines of a mapper log file.
# Keep separate statistics for each processor type.
# Compute count, total, mean and standard deviation of task weights.
# Compute effective load balance.
#
# sample input line
# [2 - 2aaaaab66700] {2}{lifeline_mapper}: 1814898282976 node 1d0002 proc 1(IO_PROC): report_profiling(1207) # <jump:fbefd> 132802750
#
# sample output
# mapper lifeline_mapper
# totalDuration xxxxx numTasks xxxxx mean xxxxx standard deviation xxxx
# proc IO_PROC numTasks xxxxx mean xxxxx standard deviation xxxx load balance xxxx
# proc PY_PROC numTasks xxxxx mean xxxxx standard deviation xxxxx load balance xxxx
#



import fileinput
import math

statistics = {}
statistics["top"] = { "durations": [], "totalDuration": 0, "numTasks": 0, "mean": 0, "standardDeviation": 0 }
balance = {}
mapper = None

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
        balance[procType] = { "balance": 0, "numProcs": 0 }
      key = node + ':' + proc

      for k in [ key, procType, "top" ]:
        if k not in statistics:
          statistics[k] = { "durations": [], "totalDuration": 0, "numTasks": 0, "mean": 0, "standardDeviation": 0 }
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

print "mapper", mapper
for key in sorted(statistics):
  print key, "numTasks", statistics[key]["numTasks"], "totalDuration", statistics[key]["totalDuration"], "mean", statistics[key]["mean"], "standardDeviation", statistics[key]["standardDeviation"]

for key in balance:
  print key, "balance", balance[key]


