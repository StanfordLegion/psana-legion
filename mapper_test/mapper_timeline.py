#
# Convert a trace produced by running psana_mapper into a timeline
# This consists of compressing repeating sequences of tasks
#

import argparse


def convertTrace(logfile):
  function = ''
  proc = -1
  sumActive = 0
  timeStart = 0
  enterMain = 0
  exitMain = 0
  totalActive = 0
  lastTaskEnded = None
  oldLastTaskEnded = None
  totalIdle = 0
  startupIdle = None
  sawMultiple = False
  numProcs = 2
  numTasks = 0
  
  for line in open(logfile):
    words = line.strip().split(' ')
    if len(words) != 3 and len(words) != 6:
      continue
    if words[2] == 'main':
      if words[1] == 'enter':
        enterMain = int(words[0])
        print enterMain, 'enter main'
      elif words[1] == 'exit':
        exitMain = int(words[0])
        print function, proc, '(' + str(numTasks) + ')', sumActive, ', (', timeStart, ':', lastTaskEnded, ')'
        print exitMain, 'exit main'
        shutdownIdle = exitMain - lastTaskEnded

    else:
      if function != words[1] or proc != words[4]:
        
        if oldLastTaskEnded == None:
          oldLastTaskEnded = enterMain
        
        if function != '':
          print function, proc, '(' + str(numTasks) + ')', sumActive, ', (', timeStart, ':', lastTaskEnded, ')'
          if startupIdle == None:
            startupIdle = timeStart - oldLastTaskEnded

        timeStart = int(words[0])
        totalActive = totalActive + sumActive
        sumActive = int(words[5])
        oldLastTaskEnded = lastTaskEnded
        function = words[1]
        proc = words[4]
        sawMultiple = False
        numTasks = 0
      else:
        sumActive = sumActive + int(words[5])
        sawMultiple = True
      
      lastTaskEnded = int(words[5]) + int(words[0])
      numTasks = numTasks + 1

  totalElapsed = int(exitMain) - int(enterMain)

  print 'totalElapsed', totalElapsed * numProcs, 'totalActive', totalActive, 'startupIdle', startupIdle, 'shutdownIdle', shutdownIdle
  print 'percent idle startup', float(startupIdle) / float(totalElapsed * numProcs)
  print 'percent idle shutdown', float(shutdownIdle) / float(totalElapsed * numProcs)
  print 'percent active utilization', float(totalActive) / float(totalElapsed * numProcs)

def driver():
  parser = argparse.ArgumentParser(description = 'Timeline for psana_mapper')
  parser.add_argument('-l', '--log', dest='logfile', action='store',
                      help='Name of log file from running psana_mapper test.rg')
  args = parser.parse_args()
  convertTrace(**vars(args))

if __name__ == '__main__':
  driver()
