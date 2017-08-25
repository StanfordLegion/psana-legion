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

import itertools
import legion
import numpy
import psana

run_number = 54

# These are initialized on every core at the beginning of time.
ds = psana.DataSource('exp=xpptut15:run=%s:rax' % run_number)
calib = None

# User configurable analysis and filter predicate.
analysis = None
predicate = None
def start(a, p = None):
    global analysis, predicate
    analysis = a
    predicate = p

class Location(object):
    __slots__ = ['filenames', 'offsets',
                 'calib_filename', 'calib_offset']
    def __init__(self, event):
        offset = event.get(psana.EventOffset)
        self.filenames = offset.filenames()
        self.offsets = offset.offsets()
        self.calib_filename = offset.lastBeginCalibCycleFilename()
        self.calib_offset = offset.lastBeginCalibCycleOffset()
    def __repr__(self):
        return 'Location(%s, %s)' % (self.offsets, self.filenames)

@legion.task
def analyze_leaf(loc):
    print('fetch', loc)

    runtime = long(legion.ffi.cast("unsigned long long", legion._my.ctx.runtime_root))
    ctx = long(legion.ffi.cast("unsigned long long", legion._my.ctx.context_root))

    loc_calib = loc.calib_filename, loc.calib_offset
    global calib
    if calib != loc_calib:
        ds.jump(loc_calib[0], loc_calib[1], runtime, ctx)
        calib = loc_calib

    event = ds.jump(loc.filenames, loc.offsets, runtime, ctx) # Fetches the data
    analysis(event) # Performs user analysis
    return True

# FIXME: This extra indirection (with a blocking call) is to work around a freeze
@legion.task(inner=True)
def analyze(loc):
    analyze_leaf(loc).get()

def chunk(iterable, chunksize):
    it = iter(iterable)
    while True:
        value = [next(it)]
        value.extend(itertools.islice(it, chunksize-1))
        yield value

# Define the main Python task. This task is called from C++. See
# top_level_task in psana_legion.cc.
@legion.task(inner=True)
def main_task():
    assert analysis is not None
    assert predicate is not None

    ds2 = psana.DataSource('exp=xpptut15:run=%s:smd' % run_number)

    chunksize = 10
    for events in chunk(itertools.ifilter(predicate, ds2.events()), chunksize):
        for event in events:
            analyze(Location(event))
        # for idx in legion.IndexLaunch([len(events)]):
        #     analyze(Location(events[idx]))
        break
