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

import legion
from legion import task, RW

from psana.psexp.smdreader_manager import SmdReaderManager
from psana.psexp.eventbuilder_manager import EventBuilderManager
from psana.psexp.event_manager import EventManager

@task(privileges=[None, RW])
def run_smd0_task(run, data, part):
    global_procs = legion.Tunable.select(legion.Tunable.GLOBAL_PYS).get()

    smdr_man = SmdReaderManager(run.smd_dm.fds, run.max_events)
    for i, chunk in enumerate(smdr_man.chunks()):
        point = 0
        if global_procs > 1:
            point = i % (global_procs - 1) + 1
        run_smd_task(chunk, run, data, part, point=point)

@task(privileges=[None, None, RW])
def run_smd_task(view, run, data, part):
    eb_man = EventBuilderManager(run.smd_configs, run.batch_size, run.filter_callback)
    for batch in eb_man.batches(view):
        run_bigdata_task(batch, run, data, part)

@task(privileges=[None, None, RW])
def run_bigdata_task(batch, run, data, part):
    evt_man = EventManager(run.smd_configs, run.dm, run.filter_callback)
    for event in evt_man.events(batch):
        run.event_fn(event, data, run.det)

def load_data(run, data, part, event_fn=None, det=None):
    run.event_fn = event_fn
    run.start_run_fn = None
    run.det = det
    run_smd0_task(run, data, part)
