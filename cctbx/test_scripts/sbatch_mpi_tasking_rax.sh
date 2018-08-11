#!/bin/bash
#SBATCH --time=00:30:00
#SBATCH --partition=debug # regular
#SBATCH --constraint=knl,quad,cache
#SBATCH --core-spec=4
#SBATCH --image=docker:stanfordlegion/cctbx-mpi-tasking:latest
#SBATCH --exclusive # causes shifter to preload image before run
#SBATCH --mail-type=ALL
#SBATCH --account=m2859

# temporarily disable the following sbatch flags:
# --job-name=psana_legion
# --dependency=singleton

# Host directory where Psana is located
# (Needed for native Legion shared library)
export ORIG_PSANA_DIR=$HOME/psana_legion/psana-legion/psana_legion
# export HOST_PSANA_DIR=$HOME/psana_legion/psana-legion/psana_legion
# export HOST_PSANA_DIR=$SCRATCH/psana_legion_mirror
export HOST_PSANA_DIR=/tmp/psana_legion

srun -n $SLURM_JOB_NUM_NODES --ntasks-per-node 1 mkdir -p $HOST_PSANA_DIR/scripts
srun -n $SLURM_JOB_NUM_NODES --ntasks-per-node 1 mkdir -p $HOST_PSANA_DIR/lib64

pushd $ORIG_PSANA_DIR
for f in psana_legion *.so *.py scripts/*.sh lib64/*; do
  sbcast -p ./$f $HOST_PSANA_DIR/$f
done
popd

# Host directory where data is located
# HOST_DATA_DIR=$SCRATCH/data/reg
# HOST_DATA_DIR=$SCRATCH/stripe_24_data/reg
# HOST_DATA_DIR=$SCRATCH/noepics_data/reg
HOST_DATA_DIR=$SCRATCH/demo_data/reg

export SIT_PSDM_DATA=$HOST_DATA_DIR/d/psdm

export IN_DIR=$PWD/input

export EAGER=1
# export LIMIT=1024
export REPEAT=1
export CHUNKSIZE=1

export PSANA_FRAMEWORK=mpi

# setting from Chris to avoid intermittent failures in PMI_Init_threads on large numbers of nodes
export PMI_MMAP_SYNC_WAIT_TIME=600 # seconds

for n in $SLURM_JOB_NUM_NODES; do
  for c in 4; do
    export LIMIT=$(( 16 * n * c ))

    # export OUT_DIR=$PWD/output_mpi_tasking_"$SLURM_JOB_ID"_n${n}_c${c}
    export OUT_DIR=$SCRATCH/cori-cctbx/output_mpi_tasking_"$SLURM_JOB_ID"_n${n}_c${c}
    mkdir -p $OUT_DIR

    echo "Running $(basename "$OUT_DIR")"

    # $HOST_PSANA_DIR/scripts/make_nodelist.py $c > $OUT_DIR/nodelist.txt
    # export SLURM_HOSTFILE=$OUT_DIR/nodelist.txt

    # srun -n $(( n * c + 1 )) -N $(( n + 1 )) --cpus-per-task $(( 256 / c )) --cpu_bind cores --distribution=arbitrary \
    srun -n $(( n * c )) -N $(( n )) --cpus-per-task $(( 256 / c )) --cpu_bind cores \
      shifter ./index_mpi_tasking.sh cxid9114 108 0 # 95 89 lustre
  done
done
