#!/bin/bash
#SBATCH --nodes=1
#SBATCH --time=00:30:00
#SBATCH --partition=debug # regular
#SBATCH --constraint=knl,quad,cache
#SBATCH --core-spec=4
#SBATCH --image=docker:stanfordlegion/cctbx-legion:latest
#SBATCH --exclusive # causes shifter to preload image before run
#SBATCH --mail-type=ALL
#SBATCH --account=m2859

# temporarily disable the following sbatch flags:
# --job-name=psana_legion
# --dependency=singleton

# Host directory where Psana is located
# (Needed for native Legion shared library)
export HOST_PSANA_DIR=$HOME/psana_legion/psana-legion

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

export PSANA_MAPPER=lifeline

export GASNET_GNI_FMA_SHARING=1
export MPICH_GNI_FMA_SHARING=enabled

export REALM_BACKTRACE=1
export GASNET_BACKTRACE=1

# setting from Chris to avoid intermittent failures in PMI_Init_threads on large numbers of nodes
export PMI_MMAP_SYNC_WAIT_TIME=600 # seconds

set -x

for n in $SLURM_JOB_NUM_NODES; do
  for c in 4; do
    export LIMIT=$(( 16 * n * c ))

    export MAX_TASKS_IN_FLIGHT=$(( 1280 / c ))

    # export OUT_DIR=$PWD/output_legion_"$SLURM_JOB_ID"_n${n}_c${c}
    export OUT_DIR=$SCRATCH/cori-cctbx/output_legion_"$SLURM_JOB_ID"_n${n}_c${c}
    mkdir -p $OUT_DIR

    echo "Running $(basename "$OUT_DIR")"

    # $HOST_PSANA_DIR/psana_legion/scripts/make_nodelist.py $c > $OUT_DIR/nodelist.txt
    # export SLURM_HOSTFILE=$OUT_DIR/nodelist.txt

    lmbsize=$(( 1024 * 32 * 32 / ( n * c ) )) # start shrinking at > 32 nodes * 32 ranks/node
    if [[ $lmbsize -gt 1024 ]]; then
        lmbsize=1024 # default is 1024 KB, don't go over default
    fi

    # srun -n $(( n * c + 1 )) -N $(( n + 1 )) --cpus-per-task $(( 256 / c )) --cpu_bind cores --distribution=arbitrary \
    srun -n $(( n * c )) -N $(( n )) --cpus-per-task $(( 256 / c )) --cpu_bind cores \
      shifter ./index_legion.sh cxid9114 108 0 \
        -ll:cpu 0 -ll:py 1 -ll:io 1 -ll:concurrent_io 1 -ll:csize $(( 48000 / c )) -ll:rsize 0 -ll:gsize 0 -ll:ib_rsize 0 -ll:lmbsize $lmbsize -lg:window 100 -level announce=2 -logfile "$OUT_DIR/ann_%.log"
    # -hl:prof $(( n * c )) -hl:prof_logfile "$OUT_DIR/prof_%.gz" 
  done
done
