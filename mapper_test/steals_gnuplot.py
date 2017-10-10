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
    thief = words[9]
    source_proc = int(source[-1])
    if source_proc == int(proc):
      thief_proc = int(thief[-1])
      output.write(str(t) + " " + str(thief_proc) + " 10\n");
  output.close()

  script = open('make.taskpool_' + proc + '.gnuplot', 'w')
  script.write('set terminal png transparent enhanced font "arial,10" fontscale 1.0 size 600, 400\n')
  script.write("set output 'taskpool_timeline_" + proc + ".png'\n")
  script.write("set title 'steals from taskpool proc " + proc + "'\n")
  script.write("set xrange [30711055000000.0:30711063000000.0]\n")
  script.write("set yrange [0:]\n")
  script.write("set ylabel 'thief processor id (3 or 4)'\n")
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
