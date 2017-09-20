import psana
from mpi4py import MPI
import numpy
import itertools
import os

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
    limit = int(os.environ['SLURM_JOB_NUM_NODES']) * 5000
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

    chunksize = 4 # Number of events per task

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

    print('Elapsed time: %e seconds' % (stop - start))
    print('Number of calib cycles: %s' % ncalib)
    print('Number of events: %s' % nevents)
    print('Events per second: %e' % (nevents/(stop - start)))

    # Hack: Estimate bandwidth used

    total_events = 75522
    total_size = 875 # GB

    fraction_events = float(nevents)/total_events
    bw = fraction_events * total_size / (stop - start)
    print('Estimated bandwidth used: %e GB/s' % bw)

else:
    ds = psana.DataSource('exp=cxid9114:run=%s:rax' % run_number)
    det = psana.Detector('CxiDs2.0:Cspad.0', ds.env())

    while True:
        MPI.COMM_WORLD.send(rank, dest=0)
        chunk = MPI.COMM_WORLD.recv(source=0)
        if chunk == 'end': break

        locs, calib = chunk
        for loc in locs:
            evt = ds.jump(loc.filenames, loc.offsets, calib)
