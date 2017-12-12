#!/bin/bash
export LG_RT_DIR=/legion/runtime
export LEGION_PATH=/legion
export PSANA_MAPPER=task_pool
alias install_mpi="conda install -y --channel conda-forge 'mpich>=3'"
alias setup_python_docker="FORCE_PYTHON=1 PYTHON_VERSION_MAJOR=2 PYTHON_LIB=/conda/lib/libpython2.7.so make"
alias run_psana="EAGER=1 REALM_SYNTHETIC_CORE_MAP= mpirun -n 2 ./psana_legion -ll:py 1 -ll:io 8 -ll:csize 6000 -lg:window 50"
