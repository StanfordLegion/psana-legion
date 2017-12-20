pushd ../psana_legion && EAGER=1 REALM_SYNTHETIC_CORE_MAP= mpirun -n 2 ./psana_legion -ll:py 1 -ll:io 1 -ll:csize 6000 -lg:window 50 -level lifeline_mapper=1
