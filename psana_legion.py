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

ds = psana.DataSource("exp=xpptut15:run=54:idx")
det = psana.Detector("cspad")
run = ds.runs().next()

@legion.task
def fetch(time):
    event = run.event(time) # Fetches the data
    raw = det.raw(event)
    # ispace = legion.Ispace(raw.shape)
    # fspace = legion.Fspace({'raw': legion.u16, 'calib': legion.f32})
    # region = legion.Region(ispace, fspace)
    # region.raw.copy(raw)
    region = None

    metadata = {'time': time}
    return metadata, region

@legion.task(privileges=[legion.RW])
def process(metadata, region):
    # print(region.raw.sum())
    pass

@legion.task
def analyze(nevent, time):
    metadata, region = fetch(time).get()
    process(metadata, region)

# @legion.task
def predicate(nevent, time):
    return True

# Define the main Python task. This task is called from C++. See
# top_level_task in psana_legion.cc.
@legion.task
def main_task():
    ds = psana.DataSource("exp=xpptut15:run=54:idx")
    det = psana.Detector("cspad")
    # evt = ds.events().next()
    # print(det.raw(evt))
    run = ds.runs().next()
    times = run.times()
    blocksize = 10
    for start in xrange(1): #xrange(0, len(times), blocksize):
        stop = min(start + blocksize, len(times))
        # print('block %s %s' % (start, stop))
        for nevent in xrange(start, stop):
            if predicate(nevent, times[nevent]):
                # print('  idx %s' % nevent)
                analyze(nevent, times[nevent])

# TODO 2: fetch small data and filter
