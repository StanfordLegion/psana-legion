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

import itertools
import legion
import numpy
import os
import psana
import random
import sys
import legion_HDF5

def force_numpy():
    print('about to call numpy.matmul to force loading MKL')
    sys.stdout.flush()
    y = numpy.ones([2, 2])
    numpy.matmul(y, y)
    print('returned from call to numpy.matmul to force loading MKL')
    sys.stdout.flush()
    return y
force_numpy()

# User configurable analysis and filter predicate.
class Config(object):
    __slots__ = ['analysis', 'predicate', 'teardown', 'limit']
    def __init__(self):
        self.analysis = None
        self.predicate = None
        self.teardown = None
        self.limit = None

class LegionSmallData(object):
    __slots__ = ['legionDataSource', 'filepath', 'gather_interval', 'data', 'hdf5']
    def __init__(self, legionDataSource, filepath, gather_interval):
        self.legionDataSource = legionDataSource
        self.filepath = filepath
        self.gather_interval = gather_interval
        self.data = []
    
    def event(self, **kwargs):
        if kwargs is not None:
            for key, value in kwargs.iteritems():
                dict = {}
                dict[key] = value
                self.data.append(dict)


_ds = None
class LegionDataSource(object):
    __slots__ = ['descriptor', 'ds_rax', 'ds_smd', 'config', 'small_data']
    def __init__(self, descriptor):
        if ':rax' not in descriptor:
            raise Exception('LegionDataSource requires RAX mode')
        self.descriptor = descriptor

        self.ds_rax = psana.DataSource(self.descriptor)
        self.ds_smd = None
        self.config = Config()
        self.small_data = None

    def rax(self):
        return self.ds_rax

    def env(self):
        return self.rax().env()

    def runs(self):
        return self.rax().runs()

    def jump(self, *args):
        return self.rax().jump(*args)

    def smd(self):
        if self.ds_smd is None:
            descriptor = self.descriptor.replace(':rax', ':smd')
            self.ds_smd = psana.DataSource(descriptor)
        return self.ds_smd

    def start(self, analysis, predicate=None, teardown=None, limit=None):
        global _ds
        assert _ds is None
        _ds = self

        self.config.analysis = analysis
        self.config.predicate = predicate
        self.config.teardown = teardown
        self.config.limit = limit

    def smalldata(self, filepath, gather_interval=100):
        self.small_data = LegionSmallData(self, filepath, gather_interval)
        return self.small_data


class Location(object):
    __slots__ = ['filenames', 'offsets']
    def __init__(self, event):
        offset = event.get(psana.EventOffset)
        self.filenames = offset.filenames()
        self.offsets = offset.offsets()
    def __repr__(self):
        return 'Location(%s, %s)' % (self.offsets, self.filenames)

@legion.task
def analyze_single(loc, calib):
    runtime = long(legion.ffi.cast("unsigned long long", legion._my.ctx.runtime_root))
    ctx = long(legion.ffi.cast("unsigned long long", legion._my.ctx.context_root))

    event = _ds.jump(loc.filenames, loc.offsets, calib.get(), runtime, ctx) # Fetches the data
    _ds.config.analysis(event) # Performs user analysis
    if _ds.small_data is not None:
        return _ds.small_data.data

@legion.task(inner=True)
def analyze_chunk(locs, calib):
    if _ds.small_data is None:
        for loc in locs:
            analyze_single(loc, calib)
    else:
        futures = []
        for loc in locs:
            future = analyze_single(loc, calib)
            futures.append(future)
        dicts = []
        for future in futures:
            dict = future.get()
            dicts = dicts + dict
        return dicts

@legion.task
def teardown():
    _ds.config.teardown() # Performs user teardown

def chunk(iterable, chunksize):
    it = iter(iterable)
    while True:
        value = [next(it)]
        value.extend(itertools.islice(it, chunksize-1))
        yield value

# Define the main Python task. This task is called from C++. See
# top_level_task in psana_legion.cc.
@legion.task(top_level=True)
def main_task():
    assert _ds is not None

    events = _ds.smd().events()
    repeat = 'REPEAT' in os.environ and os.environ['REPEAT'] == '1'
    if repeat:
        assert _ds.config.limit
        events = itertools.cycle(events)
    if _ds.config.limit is not None:
        events = itertools.islice(events, _ds.config.limit)
    if _ds.config.predicate is not None:
        events = itertools.ifilter(_ds.config.predicate, events)

    eager = 'EAGER' in os.environ and os.environ['EAGER'] == '1'
    if eager:
        start = legion.c.legion_get_current_time_in_micros()
        events = list(events)
        stop = legion.c.legion_get_current_time_in_micros()

        print('Enumerating: Elapsed time: %e seconds' % ((stop - start)/1e6))
        print('Enumerating: Number of events: %s' % len(events))
        print('Enumerating: Events per second: %e' % (len(events)/((stop - start)/1e6)))

    randomize = 'RANDOMIZE' in os.environ and os.environ['RANDOMIZE'] == '1'
    print('Randomize?', randomize)
    if randomize:
        assert eager
        random.seed(123456789) # Don't actually want this to be random
        random.shuffle(events)

    # Number of events per task
    chunksize = int(os.environ['CHUNKSIZE']) if 'CHUNKSIZE' in os.environ else 8

    # Number of tasks per processor per launch
    overcommit = int(os.environ['OVERCOMMIT']) if 'OVERCOMMIT' in os.environ else 1

    # Number of Python processors
    global_procs = legion.Tunable.select(legion.Tunable.GLOBAL_PYS).get()
    local_procs = legion.Tunable.select(legion.Tunable.LOCAL_PYS).get()

    # Number of tasks per launch
    launchsize = (max(global_procs - local_procs, local_procs)) * overcommit

    print('Chunk size %s' % chunksize)
    print('Launch size %s' % launchsize)

    start = legion.c.legion_get_current_time_in_micros()

    # Group events by calib cycle so that different cycles don't mix
    events = itertools.groupby(
        events, lambda e: e.get(psana.EventOffset).lastBeginCalibCycleDgram())

    if _ds.small_data is not None:
        # create HDF5 output file
        hdf5 = legion_HDF5.LegionHDF5(_ds.small_data.filepath)

    nevents = 0
    nlaunch = 0
    ncalib = 0
    file_buffer = []
    file_buffer_length = 0
    
    for calib, calib_events in events:
        calib = legion.Future(calib)
        for launch_events in chunk(chunk(calib_events, chunksize), launchsize):
            if nlaunch % 20 == 0:
                print('Processing event %s' % nevents)
                sys.stdout.flush()
            dictsBuffer = []

            for idx in legion.IndexLaunch([len(launch_events)]):
                dicts = analyze_chunk(map(Location, launch_events[idx]), calib)
                dictsBuffer.append(dicts)
                nevents += len(launch_events[idx])
            nlaunch += 1

            if _ds.small_data is not None:
                for dicts in dictsBuffer:
                    d = dicts.get()
                    file_buffer = file_buffer + d
                    file_buffer_length = file_buffer_length + len(d)
                    if file_buffer_length >= _ds.small_data.gather_interval:
                        hdf5.append_to_file(file_buffer)
                        file_buffer = []
                        file_buffer_length = 0

        ncalib += 1

    legion.execution_fence(block=True)
    stop = legion.c.legion_get_current_time_in_micros()

    if _ds.config.teardown is not None:
        # FIXME: Should be a must-epoch launch
        for idx in legion.IndexLaunch([global_procs]):
            teardown()

    print('Elapsed time: %e seconds' % ((stop - start)/1e6))
    print('Number of calib cycles: %s' % ncalib)
    print('Number of launches: %s' % nlaunch)
    print('Number of events: %s' % nevents)
    print('Events per second: %e' % (nevents/((stop - start)/1e6)))

    # Hack: Estimate bandwidth used

    # total_events = 75522 * repeat
    # total_size = 875 * repeat # GB

    # fraction_events = float(nevents)/total_events
    # bw = fraction_events * total_size / ((stop - start)/1e6)
    # print('Estimated bandwidth used: %e GB/s' % bw)

    print('End of run')
    sys.stdout.flush()
