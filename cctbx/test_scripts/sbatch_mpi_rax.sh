#!/bin/bash
#SBATCH --nodes=1
#SBATCH --time=00:30:00
#SBATCH --partition=debug # regular
#SBATCH --constraint=knl,quad,cache
#SBATCH --core-spec=4
#SBATCH --image=docker:stanfordlegion/cctbx-mpi:latest
#SBATCH --exclusive # causes shifter to preload image before run
#SBATCH --mail-type=ALL
#SBATCH --account=m2859

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
# export LIMIT=1024

for n in 1; do
  for c in 16; do
    export LIMIT=$(( 512 * n ))

    export OUT_DIR=$PWD/output_mpi_"$SLURM_JOB_ID"_n${n}_c${c}
    mkdir $OUT_DIR

    echo "Running $(basename "$OUT_DIR")"

    # $HOST_PSANA_DIR/psana_legion/scripts/make_nodelist.py $c > $OUT_DIR/nodelist.txt
    # export SLURM_HOSTFILE=$OUT_DIR/nodelist.txt

    # srun -n $(( n * c + 1 )) -N $(( n + 1 )) --cpus-per-task $(( 256 / c )) --cpu_bind cores --distribution=arbitrary \
    srun -n $(( n * c )) -N $(( n )) --cpus-per-task $(( 256 / c )) --cpu_bind cores \
      shifter ./index_mpi.sh cxid9114 108 0 # 95 89 lustre
  done
done
