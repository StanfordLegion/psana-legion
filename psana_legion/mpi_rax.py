import psana
from mpi4py import MPI
import numpy
import os

class Location(object):
    __slots__ = ['filenames', 'offsets', 'calib']
    def __init__(self, event):
        offset = event.get(psana.EventOffset)
        self.filenames = offset.filenames()
        self.offsets = offset.offsets()
        self.calib = offset.lastBeginCalibCycleDgram()
    def __repr__(self):
        return 'Location(%s, %s)' % (self.offsets, self.filenames)

size = MPI.COMM_WORLD.Get_size()
rank = MPI.COMM_WORLD.Get_rank()

run_number = 108

if rank == 0:
    ds = psana.DataSource('exp=cxid9114:run=%s:smd' % run_number)

    limit = int(os.environ['SLURM_JOB_NUM_NODES']) * 5000

    start = MPI.Wtime()

    events = itertools.islice(ds.events(), limit)
    for nevent, event in enumerate(events):
        worker = MPI.COMM_WORLD.recv(source=MPI.ANY_SOURCE)
        MPI.COMM_WORLD.send(Location(event), dest=worker)

    for worker in xrange(size-1):
        worker = MPI.COMM_WORLD.recv(source=MPI.ANY_SOURCE)
        MPI.COMM_WORLD.send('end', dest=worker)

    stop = MPI.Wtime()

    # Compute statistics
    nevents = limit

    print('Elapsed time: %e seconds' % (stop - start))
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
        loc = MPI.COMM_WORLD.recv(source=0)
        if loc == 'end': break

        evt = ds.jump(loc.filenames, loc.offsets, loc.calib)