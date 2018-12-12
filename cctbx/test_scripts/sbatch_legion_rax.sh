#!/bin/bash
#SBATCH --time=01:00:00
#SBATCH --partition=regular
#SBATCH --constraint=knl,quad,cache
#SBATCH --core-spec=4
#SBATCH --image=docker:stanfordlegion/cctbx-legion:subprocess
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

export ORIG_LEGION_DIR=$HOME/psana_legion/legion
export HOST_LEGION_DIR=/tmp/legion

srun -n $SLURM_JOB_NUM_NODES --ntasks-per-node 1 mkdir -p /tmp/input
srun -n $SLURM_JOB_NUM_NODES --ntasks-per-node 1 mkdir -p $HOST_PSANA_DIR/scripts
srun -n $SLURM_JOB_NUM_NODES --ntasks-per-node 1 mkdir -p $HOST_PSANA_DIR/lib64
srun -n $SLURM_JOB_NUM_NODES --ntasks-per-node 1 mkdir -p $HOST_LEGION_DIR/bindings/python
srun -n $SLURM_JOB_NUM_NODES --ntasks-per-node 1 mkdir -p $HOST_LEGION_DIR/runtime/legion
srun -n $SLURM_JOB_NUM_NODES --ntasks-per-node 1 mkdir -p $HOST_LEGION_DIR/runtime/realm

for f in *.sh input/*; do
  sbcast -p ./$f /tmp/$f
done
pushd $ORIG_PSANA_DIR
for f in psana_legion *.so *.py scripts/*.sh lib64/*; do
  sbcast -p ./$f $HOST_PSANA_DIR/$f
done
popd
pushd $ORIG_LEGION_DIR
for f in bindings/python/legion.py runtime/legion.h runtime/legion/*.h runtime/realm/*.h; do
  sbcast -p ./$f $HOST_LEGION_DIR/$f
done
popd

# Host directory where data is located
HOST_DATA_DIR=$SCRATCH/demo_data/reg

export SIT_PSDM_DATA=$HOST_DATA_DIR/d/psdm

# export IN_DIR=$PWD/input
export IN_DIR=/tmp/input

export EAGER=1
# export LIMIT=1024
export REPEAT=1
export CHUNKSIZE=1

export PSANA_MAPPER=lifeline

export GASNET_GNI_FMA_SHARING=1
export MPICH_GNI_FMA_SHARING=enabled

export REALM_BACKTRACE=1
# export GASNET_BACKTRACE=1

# setting from Chris to avoid intermittent failures in PMI_Init_threads on large numbers of nodes
export PMI_MMAP_SYNC_WAIT_TIME=600 # seconds

# fix to avoid crash on 1 and 2 cores/node
export GASNET_USE_UDREG=0

# enable new GASNet cutover mode
export GASNET_GNI_AM_RVOUS_CUTOVER=1

# try to fix memory probe issue in GASNetEx development snapshot
export GASNET_MAX_SEGSIZE='1536M/P'

set -x

for n in $SLURM_JOB_NUM_NODES; do
  for shard in ${NSHARD:-4}; do
    for py in ${NPY:-4}; do
      export LIMIT=$(( 16 * n * shard * py ))

      export MAX_TASKS_IN_FLIGHT=$(( 1280 / shard / py ))

      export OUT_DIR=$SCRATCH/cori-cctbx.subprocess/output_legion_"$SLURM_JOB_ID"_n_${n}_shard_${shard}_py_${py}_io_1
      mkdir -p $OUT_DIR
      mkdir -p $OUT_DIR/backtrace

      echo "Running $(basename "$OUT_DIR")"

      # $HOST_PSANA_DIR/scripts/make_nodelist.py $shard > $OUT_DIR/nodelist.txt
      # export SLURM_HOSTFILE=$OUT_DIR/nodelist.txt

      lmbsize=$(( 512 * 32 * 32 * 32 / ( n * shard * shard ) )) # start shrinking at >= 32 nodes * 32 ranks/node
      if [[ $lmbsize -gt 1024 ]]; then
          lmbsize=1024 # default is 1024 KB, don't go over default
      fi

      csize=$(( 16000 / shard ))
      if [[ $csize -gt 8000 ]]; then
          csize=8000
      fi

      # srun -n $(( n * shard + 1 )) -N $(( n + 1 )) --cpus-per-task $(( 256 / shard )) --cpu_bind cores --distribution=arbitrary \
      srun -n $(( n * shard )) -N $(( n )) --cpus-per-task $(( 256 / shard )) --cpu_bind cores \
        shifter /tmp/index_legion.sh cxid9114 108 0 \
          -ll:cpu 0 -ll:py $py -ll:isolate_procs -ll:realm_heap_default -ll:io 1 -ll:concurrent_io 1 -ll:csize $csize -ll:rsize 0 -ll:gsize 0 -ll:ib_rsize 0 -ll:lmbsize $lmbsize -lg:window 100
          # -level announce=2,activemsg=2,allocation=2 -logfile "$OUT_DIR/ann_%.log"
          # -hl:prof $(( n * shard )) -hl:prof_logfile "$OUT_DIR/prof_%.gz"
    done
  done
done
