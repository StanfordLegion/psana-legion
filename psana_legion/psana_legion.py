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

# TODO: Add a LegionDataSource and have it wrap DataSource as appropriate
# Also, start should be a method of LegionDataSource

# These are initialized on every core at the beginning of time.
ds = psana.DataSource('exp=xpptut15:run=%s:rax' % run_number)

class Location(object):
    __slots__ = ['filenames', 'offsets', 'calib']
    def __init__(self, event):
        offset = event.get(psana.EventOffset)
        self.filenames = offset.filenames()
        self.offsets = offset.offsets()
        self.calib = offset.lastBeginCalibCycleDgram()
    def __repr__(self):
        return 'Location(%s, %s)' % (self.offsets, self.filenames)

# User configurable analysis and filter predicate.
class Config(object):
    __slots__ = ['analysis', 'predicate']
    def __init__(self):
        self.analysis = None
        self.predicate = None
_config = Config()
def start(analysis, predicate=None):
    _config.analysis = analysis
    _config.predicate = predicate

@legion.task
def analyze_leaf(loc):
    runtime = long(legion.ffi.cast("unsigned long long", legion._my.ctx.runtime_root))
    ctx = long(legion.ffi.cast("unsigned long long", legion._my.ctx.context_root))

    event = ds.jump(loc.filenames, loc.offsets, loc.calib, runtime, ctx) # Fetches the data
    _config.analysis(event) # Performs user analysis
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

@legion.task(leaf=True)
def dummy():
    return 1

# Define the main Python task. This task is called from C++. See
# top_level_task in psana_legion.cc.
@legion.task(inner=True)
def main_task():
    assert _config.analysis is not None
    assert _config.predicate is not None

    ds2 = psana.DataSource('exp=xpptut15:run=%s:smd' % run_number)

    start = legion.c.legion_get_current_time_in_micros()

    nevents = 0
    chunksize = 10
    for i, events in enumerate(chunk(itertools.ifilter(_config.predicate, ds2.events()), chunksize)):
        for event in events:
            analyze(Location(event))
        nevents += len(events)
        # for idx in legion.IndexLaunch([len(events)]):
        #     analyze(Location(events[idx]))
        # if i > 20: break

    legion.c.legion_runtime_issue_execution_fence(
        legion._my.ctx.runtime, legion._my.ctx.context)
    dummy().get()
    stop = legion.c.legion_get_current_time_in_micros()

    print('Elapsed time: %e seconds' % (stop - start)/1e6)
    print('Number of events: %s' % nevents)
    print('Events per second: %e' % nevents/((stop - start)/1e6))
