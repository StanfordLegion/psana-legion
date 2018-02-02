#!/bin/bash
#SBATCH --job-name=psana_legion
#SBATCH --nodes=4
#SBATCH --time=00:30:00
#SBATCH --partition=regular
#SBATCH --constraint=knl,quad,cache



source ~/PSANA/setup.bash
pushd ~/PSANA/psana-legion/psana_legion
export PSANA_MAPPER=lifeline
source ../local_setup/env.sh

echo PSANA_MAPPER $PSANA_MAPPER
echo SIT_PSDM_DATA $SIT_PSDM_DATA
echo EXPERIMENT $EXPERIMENT
echo DETECTOR $DETECTOR

srun -n 4 ./psana_legion -ll:py 1 -ll:io 1 -ll:csize 6000 -lg:window 100 -level taskpool_mapper=1,lifeline_mapper=1

