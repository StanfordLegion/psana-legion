#!/usr/bin/env python

# Copyright 2018 Stanford University
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
import numpy
import os
import subprocess

from psana import DataSource

root_dir = os.path.dirname(os.path.realpath(__file__))
native_kernels_h_path = os.path.join(os.path.dirname(os.path.dirname(root_dir)), 'psana_legion', 'native_kernels_tasks.h')
lifeline_mapper_h_path = os.path.join('lifeline_mapper.h')
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
sum_task = legion.extern_task(task_id=sum_task_id, privileges=[legion.RO])

c.register_lifeline_mapper()

limit = int(os.environ['LIMIT']) if 'LIMIT' in os.environ else None

# To test on 'real' bigdata:
# xtc_dir = "/reg/d/psdm/xpp/xpptut15/scratch/mona/test"
xtc_dir = os.path.join(os.getcwd(),'.tmp')
ds = DataSource('exp=xpptut13:run=1:dir=%s'%(xtc_dir), max_events=limit, det_name='xppcspad')

# FIXME: For some reason we need this loop, even though we're not going to do any analysis inside.
for run in ds.runs():
    det = ds.Detector(ds.det_name)

def event_fn(event):
    print('Analyzing event', event)
    raw = det.raw(event)
    raw_region = legion.Region.create(raw.shape, {'x': (legion.int16, 1)})
    numpy.copyto(raw_region.x, raw, casting='no')
    sum_task(raw_region)
    raw_region.destroy()
ds.analyze(event_fn=event_fn)
