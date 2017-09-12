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
import sys

# User configurable analysis and filter predicate.
class Config(object):
    __slots__ = ['analysis', 'predicate', 'limit']
    def __init__(self):
        self.analysis = None
        self.predicate = None
        self.limit = None

_ds = None
class LegionDataSource(object):
    __slots__ = ['descriptor', 'ds_rax', 'ds_smd', 'config']
    def __init__(self, descriptor):
        if not descriptor.endswith(':rax'):
            raise Exception('LegionDataSource requires RAX mode')
        self.descriptor = descriptor

        self.ds_rax = psana.DataSource(self.descriptor)
        self.ds_smd = None
        self.config = Config()

    def rax(self):
        return self.ds_rax

    def env(self):
        return self.rax().env()

    def jump(self, *args):
        return self.rax().jump(*args)

    def smd(self):
        if self.ds_smd is None:
            descriptor = self.descriptor[:-4] + ':smd'
            self.ds_smd = psana.DataSource(descriptor)
        return self.ds_smd

    def start(self, analysis, predicate=None, limit=None):
        global _ds
        assert _ds is None
        _ds = self

        self.config.analysis = analysis
        self.config.predicate = predicate
        self.config.limit = limit

class Location(object):
    __slots__ = ['filenames', 'offsets', 'calib']
    def __init__(self, event):
        offset = event.get(psana.EventOffset)
        self.filenames = offset.filenames()
        self.offsets = offset.offsets()
        self.calib = offset.lastBeginCalibCycleDgram()
    def __repr__(self):
        return 'Location(%s, %s)' % (self.offsets, self.filenames)

@legion.task
def analyze_leaf(loc):
    runtime = long(legion.ffi.cast("unsigned long long", legion._my.ctx.runtime_root))
    ctx = long(legion.ffi.cast("unsigned long long", legion._my.ctx.context_root))

    event = _ds.jump(loc.filenames, loc.offsets, loc.calib, runtime, ctx) # Fetches the data
    _ds.config.analysis(event) # Performs user analysis
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
    assert _ds is not None

    events = _ds.smd().events()
    if _ds.config.limit is not None:
        events = itertools.islice(events, _ds.config.limit)
    if _ds.config.predicate is not None:
        events = itertools.ifilter(_ds.config.predicate, events)

    overcommit = 8
    chunksize = legion.Tunable.select(legion.Tunable.GLOBAL_PYS).get() * overcommit
    while chunksize >= 64: # FIXME: Index launch breaks with chunksize >= 64
        chunksize = chunksize / 2
    print('Using chunk size %s' % chunksize)

    start = legion.c.legion_get_current_time_in_micros()

    nevents = 0
    for i, events in enumerate(chunk(events, chunksize)):
        if i % 20 == 0:
            print('Processing event %s' % nevents)
            sys.stdout.flush()

        for idx in legion.IndexLaunch([len(events)]):
            analyze(Location(events[idx]))

        nevents += len(events)

    legion.execution_fence(block=True)
    stop = legion.c.legion_get_current_time_in_micros()

    print('Elapsed time: %e seconds' % ((stop - start)/1e6))
    print('Number of events: %s' % nevents)
    print('Events per second: %e' % (nevents/((stop - start)/1e6)))

    # Hack: Estimate bandwidth used

    total_events = 75522
    total_size = 875 # GB

    fraction_events = float(nevents)/total_events
    bw = fraction_events * total_size / ((stop - start)/1e6)
    print('Estimated bandwidth used: %e GB/s' % bw)
