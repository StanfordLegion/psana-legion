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

import cffi
import legion
from legion import task, R, RW
import numpy
import os
import subprocess
import threading

from psana import DataSource

root_dir = os.path.dirname(os.path.realpath(__file__))
native_kernels_h_path = os.path.join(os.path.dirname(os.path.dirname(root_dir)), 'psana_legion', 'native_kernels_tasks.h')
lifeline_mapper_h_path = os.path.join(os.path.dirname(os.path.dirname(root_dir)), 'psana_legion', 'lifeline_mapper.h')
native_kernels_so_path = os.path.join(root_dir, 'build', 'libnative_kernels.so')
native_kernels_header = subprocess.check_output(['gcc', '-E', '-P', native_kernels_h_path]).decode('utf-8')
lifeline_mapper_header = subprocess.check_output(['gcc', '-E', '-P', lifeline_mapper_h_path]).decode('utf-8')

ffi = cffi.FFI()
ffi.cdef(native_kernels_header)
ffi.cdef(lifeline_mapper_header)
c = ffi.dlopen(native_kernels_so_path)

memory_bound_task_id = 101
cache_bound_task_id = 102
sum_task_id = 103
c.register_native_kernels_tasks(memory_bound_task_id,
                                cache_bound_task_id,
                                sum_task_id)

memory_bound_task = legion.extern_task(task_id=memory_bound_task_id)
cache_bound_task = legion.extern_task(task_id=cache_bound_task_id)
sum_task = legion.extern_task(task_id=sum_task_id, privileges=[legion.RO], return_type=legion.int64)

if legion.is_script:
    print('WARNING: unable to set mapper in script mode')
else:
    c.register_lifeline_mapper()

kernel_kind = os.environ.get('KERNEL_KIND')
kernel_uses_raw = False
if kernel_kind == 'memory':
    kernel = memory_bound_task
elif kernel_kind == 'cachex':
    kernel = cache_bound_task
elif kernel_kind == 'sum':
    kernel_uses_raw = True
    kernel = sum_task
elif kernel_kind is None:
    kernel = None
else:
    raise Exception('Unrecognized kernel kind: %s' % kernel_kind)

limit = int(os.environ['LIMIT']) if 'LIMIT' in os.environ else None

xtc_dir = os.environ['DATA_DIR']
ds = DataSource('exp=xpptut13:run=1:dir=%s'%(xtc_dir), max_events=limit, det_name='xppcspad')

###
### Data Loading
###

data_store = []
n_events_ready = 0
n_events_used = 0
data_lock = threading.Lock()

def load_event_data(event, det):
    global n_events_ready
    raw = det.raw.raw(event)
    with data_lock:
        data_store.append((event, raw))
        n_events_ready += 1

def load_run_data(run):
    det = run.Detector('xppcspad')
    run.analyze(event_fn=load_event_data, det=det)

def reset_data():
    global data_store, n_events_ready, n_events_used
    with data_lock:
        data_store = []
        n_events_ready = 0
        n_events_used = 0

###
### Solver
###

@task(privileges=[RW])
def preprocess(data):
    global data_store, n_events_used
    with data_lock:
        raw, used, ready = data_store, n_events_used, n_events_ready
        data_store = []
        n_events_used = ready

    for idx in range(used, ready):
        numpy.copyto(data.x[idx,:,:,:], raw[idx - used][1], casting='no')

@task(privileges=[R])
def solve_local(data):
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
            preprocess(part[idx])

        # Run solver.
        futures = []
        for idx in range(global_procs): # legion.IndexLaunch([global_procs]): # FIXME: index launch
            futures.append(solve_local(part[idx]))
        overall_answer = 0
        for future in futures:
            overall_answer += future.get()
        print('iteration {} result of solve is {}'.format(iteration, overall_answer))
        iteration += 1
    return overall_answer

for run in ds.runs():
    # FIXME: must epoch launch
    load_run_data(run)

    result = solve()
    print('result of solve is {}'.format(result.get()))

    legion.execution_fence(block=True)
    reset_data()

# notes:
# * what solves do we actually need
