import psana
from mpi4py import MPI
import numpy

run_number = 108
ds = psana.DataSource('exp=cxid9114:run=%s:idx' % run_number)
det = psana.Detector('CxiDs2.0:Cspad.0', ds.env())

run = ds.runs().next()
times = run.times()

limit = 5000
if limit: times = times[:limit]

times = numpy.array_split(times, MPI.COMM_WORLD.Get_size())[MPI.COMM_WORLD.Get_rank()]

MPI.COMM_WORLD.Barrier()
start = MPI.Wtime()

for time in times:
    evt = run.event(time)
    det.raw(evt) # Fetch data
    # det.calib(evt) # Calibrate data

MPI.COMM_WORLD.Barrier()
stop = MPI.Wtime()

if MPI.COMM_WORLD.Get_rank() == 0:
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
