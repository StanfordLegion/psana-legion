import psana
from mpi4py import MPI

rank = MPI.COMM_WORLD.Get_rank()
if rank == 0:

    run_number = 108
    ds = psana.DataSource('exp=cxid9114:run=%s:smd' % run_number)
    det = psana.Detector('CxiDs2.0:Cspad.0')

    start = MPI.Wtime()

    nevents = 0
    for event in ds.events():
        nevents += 1

    stop = MPI.Wtime()

    print('Elapsed time: %e seconds' % (stop - start))
    print('Number of events: %s' % nevents)
    print('Events per second: %e' % (nevents/(stop - start)))
