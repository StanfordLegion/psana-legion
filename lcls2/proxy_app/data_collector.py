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
from legion import task, RW
import numpy
import threading

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

@task(privileges=[RW])
def fill_data_region(data):
    global data_store, n_events_used
    with data_lock:
        raw, used, ready = data_store, n_events_used, n_events_ready
        data_store = []
        n_events_used = ready

    for idx in range(used, ready):
        numpy.copyto(data.x[idx,:,:,:], raw[idx - used][1], casting='no')
