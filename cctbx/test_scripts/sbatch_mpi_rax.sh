#!/bin/bash
#SBATCH --nodes=1
#SBATCH --time=00:30:00
#SBATCH --partition=debug # regular
#SBATCH --constraint=knl,quad,cache
#SBATCH --core-spec=4
#SBATCH --image=docker:stanfordlegion/cctbx-mpi:latest
#SBATCH --exclusive # causes shifter to preload image before run
#SBATCH --mail-type=ALL
#SBATCH --account=ACCOUNT

# temporarily disable the following sbatch flags:
# --job-name=psana_legion
# --dependency=singleton

# Host directory where data is located
HOST_DATA_DIR=$SCRATCH/data/reg
# HOST_DATA_DIR=$SCRATCH/stripe_24_data/reg
# HOST_DATA_DIR=$SCRATCH/noepics_data/reg

export SIT_PSDM_DATA=$HOST_DATA_DIR/d/psdm

t_start=`date +%s`
# FIXME (Elliott): Does the trial number matter????
for n in 4; do # 8
  echo "Running n$n"
  srun -n $n -N 1 --cpus-per-task $(( 256 / n )) --cpu_bind cores \
    shifter ./index.sh cxid9114 108 0 # 95 89 lustre
done
t_end=`date +%s`
echo PSJobCompleted TotalElapsed $((t_end-t_start)) $t_start $t_end
