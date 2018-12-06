#!/usr/bin/env python

from __future__ import print_function

import os
import subprocess
import sys

assert len(sys.argv) == 2
c = int(sys.argv[1]) # Number of ranks per node

reserve = 1 # Number of ranks to be given dedicated nodes

nodelist = subprocess.check_output(
    ['scontrol', 'show', 'hostname', os.environ['SLURM_JOB_NODELIST']]).decode('utf-8').strip().split()
first = nodelist[:reserve]
rest = [x for node in nodelist[reserve:] for x in [node]*c]
print('\n'.join(first+rest))
