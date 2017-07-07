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
import numpy
import psana

# These are initialized on every core at the beginning of time.
ds = psana.DataSource("exp=xpptut15:run=54:idx")
det = psana.Detector("cspad")
run = ds.runs().next()

@legion.task
def fetch(time):
    event = run.event(time) # Fetches the data
    raw = det.raw(event)
    region = legion.Region.create(
        raw.shape,
        {'raw': legion.int16, 'calib': legion.float32})
    numpy.copyto(region.raw, raw, casting='no')

    metadata = {'time': time}
    return region, metadata

@legion.task(privileges=[legion.RW])
def process(region, metadata):
    print(region.raw.sum())

@legion.task
def analyze(nevent, time):
    region, metadata = fetch(time).get()
    process(region, metadata)
    region.destroy()

# This is so short it's not worth running as a task.
# @legion.task
def predicate(nevent, time):
    return True

# Define the main Python task. This task is called from C++. See
# top_level_task in psana_legion.cc.
@legion.task
def main_task():
    times = run.times()
    blocksize = 10
    nevents = 10 # len(times)
    for start in xrange(0, nevents, blocksize):
        stop = min(start + blocksize, nevents)
        for nevent in xrange(start, stop):
            # small = fetch_small(times[nevent]) # TODO: Fetch small data.
            if predicate(nevent, times[nevent]):
                analyze(nevent, times[nevent])
