#!/bin/bash
#SBATCH --time=01:00:00
#SBATCH --qos=regular
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

TMP_DIR=$SCRATCH/tmp
mkdir -p $TMP_DIR
INPUT_TAR=$TMP_DIR/input_$SLURM_JOB_ID.tar
PSANA_TAR=$TMP_DIR/psana_$SLURM_JOB_ID.tar

tar cfv $INPUT_TAR *.sh input/*
pushd $ORIG_PSANA_DIR
tar cfv $PSANA_TAR --transform 's#^#psana_legion/#' --show-transformed-names psana_legion *.so *.py scripts/*.sh lib64/*
popd

sbcast -f $INPUT_TAR /tmp/input.tar
sbcast -f $PSANA_TAR /tmp/psana.tar
srun -n $SLURM_JOB_NUM_NODES --ntasks-per-node 1 bash -c "tar xf /tmp/input.tar -C /tmp && tar xf /tmp/psana.tar -C /tmp"

# Host directory where data is located
# HOST_DATA_DIR=$SCRATCH/data/reg
# HOST_DATA_DIR=$SCRATCH/stripe_24_data/reg
# HOST_DATA_DIR=$SCRATCH/noepics_data/reg
HOST_DATA_DIR=$SCRATCH/demo_data/reg

export SIT_PSDM_DATA=$HOST_DATA_DIR/d/psdm

# export IN_DIR=$PWD/input
export IN_DIR=/tmp/input

export EAGER=1
# export LIMIT=1024
export REPEAT=1
export CHUNKSIZE=1

export PSANA_FRAMEWORK=mpi

# setting from Chris to avoid intermittent failures in PMI_Init_threads on large numbers of nodes
export PMI_MMAP_SYNC_WAIT_TIME=600 # seconds

for n in $(( SLURM_JOB_NUM_NODES - 1 )); do
  for shard in ${NSHARD:-4}; do
    export LIMIT=$(( 16 * n * shard ))

    export OUT_DIR=$SCRATCH/cori-cctbx.subprocess/output_mpi_"$SLURM_JOB_ID"_n_${n}_shard_${shard}_py__io_
    mkdir -p $OUT_DIR

    echo "Running $(basename "$OUT_DIR")"

    $ORIG_PSANA_DIR/scripts/make_nodelist.py $shard > $OUT_DIR/nodelist.txt
    export SLURM_HOSTFILE=$OUT_DIR/nodelist.txt

    # srun -n $(( n * shard )) -N $(( n )) --cpus-per-task $(( 256 / shard )) --cpu_bind cores \
    srun -n $(( n * shard + 1 )) -N $(( n + 1 )) --cpus-per-task $(( 256 / shard )) --cpu_bind cores --distribution=arbitrary \
      shifter ./index_mpi_tasking.sh cxid9114 108 0 # 95 89 lustre
  done
done
