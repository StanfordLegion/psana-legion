
import fileinput, sys

found = {}

for line in fileinput.input():
  words = line.split(' ')
  function = words[9]
  if function.startswith('map_task'):
    taskname = words[13]
    if taskname in found:
      print "duplicate", taskname
      sys.exit()
    found[taskname] = True
    print "mapped  ", taskname
  if function.startswith('report_profiling'):
    taskname = words[11]
    if taskname not in found:
      print "unmatched task", taskname
    else:
      if found[taskname] == True:
        found[taskname] = False
        print "reported", taskname
      else:
        print "task reported twice", taskname

for f in found:
  if found[f]:
    print "unreported", f
