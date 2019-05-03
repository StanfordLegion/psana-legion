#!/usr/bin/env python

# Copyright 2019 Stanford University
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

from __future__ import print_function

from psana import DataSource
import legion
from legion import task
import os

import native_tasks
import data_collector
import solver

import numpy as np
import os.path
from numpy import fft

# At this point, we cannot have a realistic algorithm that collects
# realistic data and solves the phasing problem.
# Therefore, this program divides the problem:
#  - it loads realistic XPP data and applies a trivial solve;
#  - it generates some 3D data and applies a realistic phasing solve.


# FIXME: this crashes if I don't define at least one task here....
@task
def dummy():
    pass


limit = int(os.environ['LIMIT']) if 'LIMIT' in os.environ else None
n_gen_reconstructions = 2

xtc_dir = os.environ['DATA_DIR']
ds = DataSource('exp=xpptut13:run=1:dir=%s'%(xtc_dir), max_events=limit, det_name='xppcspad')


for run in ds.runs():
    # FIXME: must epoch launch
    data_collector.load_run_data(run)
    # Right now, we assume one run or a serie of runs with the same
    # experimental configuration.


global_procs = legion.Tunable.select(legion.Tunable.GLOBAL_PYS).get()
n_procs_xpp = max(1, global_procs-n_gen_reconstructions)

result_xpp = solver.solve_xpp(n_procs_xpp)
results_gen = []
for i in range(n_gen_reconstructions):
    results_gen.append(solver.solve_gen(i))

print('Result of XPP solve is {}'.format(result_xpp.get()))
for i, result in enumerate(results_gen):
    print('Result of Gen solve #{} is {}'.format(
        i, result.get()))

legion.execution_fence(block=True)
data_collector.reset_data()
