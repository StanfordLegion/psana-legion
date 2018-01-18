#!/bin/bash
#SBATCH --job-name=psana_legion
#SBATCH --nodes=4
#SBATCH --time=00:30:00
#SBATCH --partition=regular
#SBATCH --constraint=knl,quad,cache



source ~/PSANA/setup.bash
pushd ~/PSANA/psana-legion/psana_legion
export PSANA_MAPPER=task_pool
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:`pwd`
export PYTHON_PATH=${PYTHON_PATH}:`pwd`

srun -n 4 ./psana_legion -ll:py 1 -ll:io 1 -ll:csize 6000 -lg:window 100 -level taskpool_mapper=1

