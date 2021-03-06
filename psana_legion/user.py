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

import os

import psana
import numpy
if os.environ.get('PSANA_FRAMEWORK') == 'mpi':
    import psana_mpi as psana_legion
    legion = None
else:
    import psana_legion
    import legion

# Get the analysis kernel to perform on each event
kernel_kind = os.environ.get('KERNEL_KIND')
kernel_uses_raw = False
if kernel_kind == 'memory_bound':
    import kernels
    kernel = kernels.make_memory_bound_kernel(int(os.environ.get('KERNEL_ROUNDS', 100)))
elif kernel_kind == 'memory_bound_native':
    if legion is not None:
        kernel = legion.extern_task(task_id=2)
    else:
        import native_kernels
        kernel = native_kernels.memory_bound_kernel
elif kernel_kind == 'cache_bound_native':
    if legion is not None:
        kernel = legion.extern_task(task_id=3)
    else:
        import native_kernels
        kernel = native_kernels.cache_bound_kernel
elif kernel_kind == 'sum':
    kernel_uses_raw = True
    if legion is not None:
        kernel = legion.extern_task(task_id=4, privileges=[legion.RO])
    else:
        assert False
elif kernel_kind is None:
    kernel = None
else:
    raise Exception('Unrecognized kernel kind: %s' % kernel_kind)

dop = int(os.environ.get('KERNEL_DOP', 1))
if kernel is not None and dop > 1:
    def make_parallel(thunk):
        def new_kernel():
            for _ in xrange(dop):
                thunk()
        return new_kernel
    kernel = make_parallel(kernel)
    print('Kernel DOP is %s' % dop)

experiment = os.environ['EXPERIMENT'] if 'EXPERIMENT' in os.environ else ('exp=cxid9114:run=%s:rax' % 108)
detector = os.environ['DETECTOR'] if 'DETECTOR' in os.environ else 'CxiDs2.0:Cspad.0'
ds = psana_legion.LegionDataSource(experiment)
det = psana.Detector(detector, ds.env())
small_data = None
if os.environ.get('TEST_HDF5') == '1':
    small_data = ds.smalldata('TEST.HDF5', gather_interval = 10)

dummy = 0

import numpy

def analyze(event):
    # raw = det.raw(event)
    # calib = det.calib(event) # Calibrate the data

    if kernel is not None:
        if kernel_uses_raw:
            raw = det.raw(event)
            if raw is None:
                print('WARNING: received event with no raw data')
            else:
                raw_region = legion.Region.create(raw.shape, {'x': (legion.int16, 1)})
                numpy.copyto(raw_region.x, raw, casting='no')
                kernel(raw_region)
        else:
            kernel()
    if small_data is not None:
        global dummy
        small_data.event(dummy=[numpy.array([dummy])]) # debugging
        dummy = dummy + 1

def filter(event):
    return True

limit = int(os.environ['LIMIT']) if 'LIMIT' in os.environ else None
ds.start(analyze, filter, limit=limit)
