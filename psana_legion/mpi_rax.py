from __future__ import print_function

import psana
from mpi4py import MPI
import numpy
import itertools
import os
import random

# Get the analysis kernel to perform on each event
kernel_kind = os.environ.get('KERNEL_KIND')
if kernel_kind == 'memory_bound':
    import kernels
    kernel = kernels.make_memory_bound_kernel(int(os.environ.get('KERNEL_ROUNDS', 100)))
elif kernel_kind == 'memory_bound_native':
    import native_kernels
    kernel = native_kernels.memory_bound_kernel
elif kernel_kind is None:
    kernel = None
else:
    raise Exception('Unrecognized kernel kind: %s' % kernel_kind)

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

size = MPI.COMM_WORLD.Get_size()
rank = MPI.COMM_WORLD.Get_rank()

run_number = 108

if rank == 0:
    ds = psana.DataSource('exp=cxid9114:run=%s:smd' % run_number)

    events = ds.events()
    limit = int(os.environ['LIMIT']) if 'LIMIT' in os.environ else None
    if limit:
        events = itertools.islice(events, limit)

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
    print('Randomize? %s' % randomize)
    if randomize:
        assert eager
        random.seed(123456789) # Don't actually want this to be random
        random.shuffle(events)

    # Number of events per task
    chunksize = int(os.environ['CHUNKSIZE']) if 'CHUNKSIZE' in os.environ else 4

    start = MPI.Wtime()

    # Group events by calib cycle so that different cycles don't mix
    events = itertools.groupby(
        events, lambda e: e.get(psana.EventOffset).lastBeginCalibCycleDgram())

    nevents = 0
    ncalib = 0
    for calib, calib_events in events:
        for chunk_events in chunk(calib_events, chunksize):
            worker = MPI.COMM_WORLD.recv(source=MPI.ANY_SOURCE)
            MPI.COMM_WORLD.send((map(Location, chunk_events), calib), dest=worker)
            nevents += len(chunk_events)
        ncalib += 1

    for worker in xrange(size-1):
        worker = MPI.COMM_WORLD.recv(source=MPI.ANY_SOURCE)
        MPI.COMM_WORLD.send('end', dest=worker)

    stop = MPI.Wtime()

    total_kernel_time = MPI.COMM_WORLD.reduce(0)

    print('Elapsed time: %e seconds' % (stop - start))
    print('Number of calib cycles: %s' % ncalib)
    print('Number of events: %s' % nevents)
    print('Events per second: %e' % (nevents/(stop - start)))

    print('Total kernel time: %e seconds' % total_kernel_time)
    print('Time per kernel call: %e seconds' % (total_kernel_time/nevents))

    # Hack: Estimate bandwidth used

    total_events = 75522 * repeat
    total_size = 875 * repeat # GB

    fraction_events = float(nevents)/total_events
    bw = fraction_events * total_size / (stop - start)
    print('Estimated bandwidth used: %e GB/s' % bw)

else:
    ds = psana.DataSource('exp=cxid9114:run=%s:rax' % run_number)
    det = psana.Detector('CxiDs2.0:Cspad.0', ds.env())

    total_kernel_time = 0
    while True:
        MPI.COMM_WORLD.send(rank, dest=0)
        chunk = MPI.COMM_WORLD.recv(source=0)
        if chunk == 'end': break

        locs, calib = chunk
        for loc in locs:
            evt = ds.jump(loc.filenames, loc.offsets, calib)
            if kernel is not None:
                kernel_start = MPI.Wtime()
                kernel()
                kernel_stop = MPI.Wtime()
                total_kernel_time += kernel_stop - kernel_start

    MPI.COMM_WORLD.reduce(total_kernel_time)
