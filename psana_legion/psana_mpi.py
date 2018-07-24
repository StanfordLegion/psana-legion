#!/usr/bin/env python

# Copyright 2018 Stanford University
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
from mpi4py import MPI
import numpy
import os
import psana
import random
import sys

# User configurable analysis and filter predicate.
class Config(object):
    __slots__ = ['analysis', 'predicate', 'teardown', 'limit']
    def __init__(self):
        self.analysis = None
        self.predicate = None
        self.teardown = None
        self.limit = None

class Location(object):
    __slots__ = ['filenames', 'offsets']
    def __init__(self, event):
        offset = event.get(psana.EventOffset)
        self.filenames = offset.filenames()
        self.offsets = offset.offsets()
    def __repr__(self):
        return 'Location(%s, %s)' % (self.offsets, self.filenames)

def chunk(iterable, chunksize):
    it = iter(iterable)
    while True:
        value = [next(it)]
        value.extend(itertools.islice(it, chunksize-1))
        yield value

_ds = None
class LegionDataSource(object):
    __slots__ = ['descriptor', 'ds_rax', 'ds_smd', 'config']
    def __init__(self, descriptor):
        if ':rax' not in descriptor:
            raise Exception('LegionDataSource requires RAX mode')
        self.descriptor = descriptor

        self.ds_rax = psana.DataSource(self.descriptor)
        self.ds_smd = None
        self.config = Config()

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
        self.config.analysis = analysis
        self.config.predicate = predicate
        self.config.teardown = teardown
        self.config.limit = limit

        rank = MPI.COMM_WORLD.Get_rank()
        if rank == 0:
            self.server()
        else:
            self.client()

    def analyze_single(self, loc, calib):
        event = self.jump(loc.filenames, loc.offsets, calib) # Fetches the data
        self.config.analysis(event) # Performs user analysis

    def analyze_chunk(self, locs, calib):
        for loc in locs:
            self.analyze_single(loc, calib)

    def teardown(self):
        self.config.teardown() # Performs user teardown

    def client(self):
        rank = MPI.COMM_WORLD.Get_rank()

        while True:
            MPI.COMM_WORLD.send(rank, dest=0)
            chunk = MPI.COMM_WORLD.recv(source=0)
            if chunk == 'end': break

            self.analyze_chunk(*chunk)

        if self.config.teardown is not None:
            self.teardown()

    def server(self):
        events = self.smd().events()
        if self.config.limit is not None:
            events = itertools.islice(events, self.config.limit)
        if self.config.predicate is not None:
            events = itertools.ifilter(self.config.predicate, events)

        eager = 'EAGER' in os.environ and os.environ['EAGER'] == '1'
        if eager:
            start = MPI.Wtime()
            events = list(events)
            stop = MPI.Wtime()

            print('Enumerating: Elapsed time: %e seconds' % (stop - start))
            print('Enumerating: Number of events: %s' % len(events))
            print('Enumerating: Events per second: %e' % (len(events)/(stop - start)))

        repeat = int(os.environ['REPEAT']) if 'REPEAT' in os.environ else 1
        if repeat > 1:
            assert eager
            events = events * repeat

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
        nprocs = MPI.COMM_WORLD.Get_size()

        # Number of tasks per launch
        launchsize = (nprocs - 1) * overcommit

        print('Chunk size %s' % chunksize)
        print('Launch size %s' % launchsize)

        start = MPI.Wtime()

        # Group events by calib cycle so that different cycles don't mix
        events = itertools.groupby(
            events, lambda e: e.get(psana.EventOffset).lastBeginCalibCycleDgram())

        nevents = 0
        ncalib = 0
        file_buffer = []
        file_buffer_length = 0

        for calib, calib_events in events:
            for chunk_events in chunk(calib_events, chunksize):
                worker = MPI.COMM_WORLD.recv(source=MPI.ANY_SOURCE)
                MPI.COMM_WORLD.send((map(Location, chunk_events), calib), dest=worker)
                nevents += len(chunk_events)
            ncalib += 1

        size = MPI.COMM_WORLD.Get_size()
        for worker in xrange(size-1):
            worker = MPI.COMM_WORLD.recv(source=MPI.ANY_SOURCE)
            MPI.COMM_WORLD.send('end', dest=worker)

        stop = MPI.Wtime()

        if self.config.teardown is not None:
            self.teardown()

        print('Elapsed time: %e seconds' % (stop - start))
        print('Number of calib cycles: %s' % ncalib)
        print('Number of events: %s' % nevents)
        print('Events per second: %e' % (nevents/(stop - start)))

        # Hack: Estimate bandwidth used

        total_events = 75522 * repeat
        total_size = 875 * repeat # GB

        fraction_events = float(nevents)/total_events
        bw = fraction_events * total_size / (stop - start)
        print('Estimated bandwidth used: %e GB/s' % bw)

        print('End of run')
        sys.stdout.flush()
