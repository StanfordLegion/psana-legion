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
from legion import task, RW
import numpy
import os
import subprocess

from psana import DataSource

import data_collector

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

def preprocess(event, data, det):
    raw = det.raw.raw(event)
    raw_region = legion.Region.create(raw.shape, {'x': (legion.uint16, 1)})
    legion.fill(raw_region, 'x', 0)
    numpy.copyto(raw_region.x, raw, casting='no')
    result = sum_task(raw_region)
    for i in range(data.ispace.volume):
        if not data.valid[i]:
            data.sum[i] = result.get()
            data.valid[i] = True
            break

@task(privileges=[RW])
def solve_local(data):
    return data.sum.sum()

@task(privileges=[RW])
def solve(data, part):
    futures = []
    for piece in part:
        futures.append(solve_local(piece))
    overall_answer = 0
    for future in futures:
        overall_answer += future.get()
    return overall_answer

for run in ds.runs():
    det = run.Detector('xppcspad')
    data = legion.Region.create([10000], {'sum': legion.int64, 'valid': legion.uint8})
    legion.fill(data, 'sum', -1)
    legion.fill(data, 'valid', 0)
    part = legion.Partition.create_equal(data, [10])
    data_collector.load_data(run, data, part, event_fn=preprocess, det=det)

    result = solve(data, part)
    print('result of solve is {}'.format(result.get()))

# notes:
# * what solves do we actually need
