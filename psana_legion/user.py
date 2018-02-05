#!/usr/bin/env python

# Copyright 2017 Stanford University
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
import psana_legion

# Get the analysis kernel to perform on each event
import kernels
kernel_kind = os.environ.get('KERNEL_KIND')
if kernel_kind == 'memory_bound':
    kernel = kernels.make_memory_bound_kernel(int(os.environ.get('KERNEL_ROUNDS', 100)))
elif kernel_kind is None:
    kernel = None
else:
    raise Exception('Unrecognized kernel kind: %s' % kernel_kind)

experiment = os.environ['EXPERIMENT'] if 'EXPERIMENT' in os.environ else ('exp=cxid9114:run=%s:rax' % 108)
detector = os.environ['DETECTOR'] if 'DETECTOR' in os.environ else 'CxiDs2.0:Cspad.0'
ds = psana_legion.LegionDataSource(experiment)
det = psana.Detector(detector, ds.env())

def analyze(event):
    # raw = det.raw(event)
    # calib = det.calib(event) # Calibrate the data

    if kernel is not None:
        kernel()

def filter(event):
    return True

limit = int(os.environ['LIMIT']) if 'LIMIT' in os.environ else None
ds.start(analyze, filter, limit=limit)
