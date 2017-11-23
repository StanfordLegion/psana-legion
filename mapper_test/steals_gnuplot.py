#
# Convert a trace produced by running psana_mapper into a timeline
# This consists of compressing repeating sequences of tasks
#

import argparse


def convertTrace(logfile, proc):
  output = open('make.taskpool_' + proc + '.dat', 'w')
  for line in open(logfile):
    words = line.strip().split(' ')
    t = words[5]
    source = words[7]
    thief = words[11]
    source_proc = int(source[-1])
    if source_proc == int(proc):
      thief_proc = int(thief[-1])
      output.write(str(t) + " " + str(thief_proc) + " 10\n");
  output.close()

  xmin_taskpool = open("make.taskpool.xmin", "r").readline().strip().split(" ")
  xmax_taskpool = open("make.taskpool.xmax", "r").readline().strip().split(" ")
  xmin_worker = open("make.worker.xmin", "r").readline().strip().split(" ")
  xmax_worker = open("make.worker.xmax", "r").readline().strip().split(" ")
  if long(xmin_taskpool[5]) < long(xmin_worker[0]):
    xmin = xmin_taskpool[5]
  else:
    xmin = xmin_worker[0]
  if long(xmax_taskpool[5]) > long(xmax_worker[0]):
    xmax = xmax_taskpool[5]
  else:
    xmax = xmax_worker[0]

  script = open('make.taskpool_' + proc + '.gnuplot', 'w')
  script.write('set terminal png transparent enhanced font "arial,10" fontscale 1.0 size 600, 400\n')
  script.write("set output 'taskpool_timeline_" + proc + ".png'\n")
  script.write("set title 'steals from taskpool proc " + proc + "'\n")
  script.write("set xrange [" + xmin + ".0:" + xmax + ".0]\n")
  script.write("set yrange [0:]\n")
  script.write("set ylabel 'thief processor id (3 or 4)'\n")
  script.write('plot "make.taskpool_' + proc + '.dat" with boxes\n')
  script.write("set output 'taskpool_detail_" + proc + ".png'\n")
  script.write("set xrange [" + "0" + ".0:" + xmax_taskpool[5] + ".0]\n")
  script.write("set style fill solid 1.0\n")
  script.write('plot "make.taskpool_' + proc + '.dat" with boxes\n')
  script.close()


def driver():
  parser = argparse.ArgumentParser(description = 'Sorted timeline for psana_mapper')
  parser.add_argument('-l', '--log', dest='logfile', action='store',
                      help='Name of log file from running psana_mapper test.rg | sort -n')
  parser.add_argument('-p', '--processor', dest='proc', action='store',
                      help='ID of worker processor (3 or 4)')
  args = parser.parse_args()
  convertTrace(**vars(args))

if __name__ == '__main__':
  driver()
