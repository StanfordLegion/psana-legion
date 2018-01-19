#!/bin/bash
#SBATCH --nodes=1
#SBATCH --time=00:30:00
#SBATCH --partition=debug # regular
#SBATCH --constraint=knl,quad,cache
#SBATCH --core-spec=4
#SBATCH --image=docker:stanfordlegion/cctbx-mpi:latest
#SBATCH --exclusive # causes shifter to preload image before run
#SBATCH --mail-type=ALL
#SBATCH --account=lcls

# temporarily disable the following sbatch flags:
# --job-name=psana_legion
# --dependency=singleton

# Host directory where data is located
# HOST_DATA_DIR=$SCRATCH/data/reg
# HOST_DATA_DIR=$SCRATCH/stripe_24_data/reg
# HOST_DATA_DIR=$SCRATCH/noepics_data/reg
HOST_DATA_DIR=$SCRATCH/demo_data/reg

export SIT_PSDM_DATA=$HOST_DATA_DIR/d/psdm

export IN_DIR=$PWD/input

# export EAGER=1 # Not supported
export LIMIT=1

for n in 8; do
  echo "Running n$n"

  export OUT_DIR=$PWD/output_mpi_"$SLURM_JOB_ID"_n$n
  mkdir $OUT_DIR

  srun -n $n -N 1 --cpus-per-task $(( 256 / n )) --cpu_bind cores \
    shifter ./index_mpi.sh cxid9114 108 0 # 95 89 lustre
done
