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

import legion
import psana

@legion.task
def analyze(ctx, nevent, time):
    ds = psana.DataSource("exp=xpptut15:run=54:idx")
    det = psana.Detector("cspad")
    run = ds.runs().next()

    event = run.event(time)
    print(event.get(psana.EventId))

    print('%x' % legion.c.legion_runtime_get_executing_processor(ctx.runtime, ctx.context).id)

    det.raw(event) # fetches the data
    det.calib(event) # calibrates the data
    # TODO: try to overlap fetch and calibrate

# Define the main Python task. This task is called from C++. See
# top_level_task in python_iterop.cc.
@legion.task
def main_task(ctx):
    ds = psana.DataSource("exp=xpptut15:run=54:idx")
    det = psana.Detector("cspad")
    # evt = ds.events().next()
    # print(det.raw(evt))
    run = ds.runs().next()
    times = run.times()
    for nevent, time in enumerate(times):
        # event = run.event(time)
        # print(event.get(psana.EventId))
        analyze(ctx, nevent, time)
        if nevent > 10: break

# TODO 2: fetch small data and filter
