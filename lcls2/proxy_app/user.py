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

import legion
from legion import task, R, RW
import numpy
import os

from psana import DataSource

import native_tasks
import data_collector

limit = int(os.environ['LIMIT']) if 'LIMIT' in os.environ else None

xtc_dir = os.environ['DATA_DIR']
ds = DataSource('exp=xpptut13:run=1:dir=%s'%(xtc_dir), max_events=limit, det_name='xppcspad')

###
### Solver
###

@task(privileges=[RW])
def preprocess(data):
    pass # do nothing in the preprocess phase for now

@task(privileges=[R])
def solve_step(data):
    return data.x.sum()

@task(privileges=[RW])
def solve():
    global_procs = legion.Tunable.select(legion.Tunable.GLOBAL_PYS).get()

    # Allocate data structures.
    n_events_per_node = 1000
    event_raw_shape = (2, 3, 6)
    data = legion.Region.create((n_events_per_node,) + event_raw_shape, {'x': legion.uint16})
    legion.fill(data, 'x', 0)
    part = legion.Partition.create_equal(data, [global_procs])

    iteration = 0
    overall_answer = 0
    while overall_answer == 0:
        # Obtain the newest copy of the data.
        for idx in range(global_procs): # legion.IndexLaunch([global_procs]): # FIXME: index launch
            data_collector.fill_data_region(part[idx])

        # Preprocess data.
        for idx in range(global_procs): # legion.IndexLaunch([global_procs]): # FIXME: index launch
            preprocess(part[idx])

        # Run solver.
        futures = []
        for idx in range(global_procs): # legion.IndexLaunch([global_procs]): # FIXME: index launch
            futures.append(solve_step(part[idx]))
        overall_answer = 0
        for future in futures:
            overall_answer += future.get()
        print('iteration {} result of solve is {}'.format(iteration, overall_answer))
        iteration += 1
    return overall_answer

for run in ds.runs():
    # FIXME: must epoch launch
    data_collector.load_run_data(run)

    result = solve()
    print('result of solve is {}'.format(result.get()))

    legion.execution_fence(block=True)
    data_collector.reset_data()

# notes:
# * what solves do we actually need
