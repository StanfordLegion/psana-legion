#!/bin/bash
#SBATCH --job-name=psana_legion
#SBATCH --dependency=singleton
#SBATCH --time=00:30:00
#SBATCH --qos=debug
#SBATCH --constraint=knl,quad,cache
#SBATCH --core-spec=4
#SBATCH --image=docker:stanfordlegion/psana-legion:elliott
#SBATCH --exclusive # causes shifter to preload image before run
#SBATCH --mail-type=ALL
#SBATCH --account=m2859

# Run with: sbatch --nodes=$(( N + 1 )) sbatch_legion_rax.sh

# Host directory where Psana is located
# (Needed for native Legion shared library)
HOST_PSANA_DIR=$HOME/psana_legion/psana-legion

# Host directory where Legion is located
# (Needed for Python bindings)
HOST_LEGION_DIR=$HOME/psana_legion/legion

# Host directory where data is located
HOST_DATA_DIR=$SCRATCH/data/reg

export SIT_PSDM_DATA=$HOST_DATA_DIR/d/psdm

echo "HOST_DATA_DIR=$HOST_DATA_DIR"

export EAGER=1
export PSANA_MAPPER=simple
export REPEAT=1

# PROFILE_DIR=$SCRATCH/profiles/$(basename $PWD)_slurm${SLURM_JOB_ID}
# mkdir -p $PROFILE_DIR

for n in $(( SLURM_JOB_NUM_NODES - 1 )); do
  export LIMIT=$(( n * 2048 ))
  for shard in 4 2 1; do
    for py in 16 8 4; do
      ./make_nodelist.py $shard > nodelist.txt
      export SLURM_HOSTFILE=$PWD/nodelist.txt
      export MAX_TASKS_IN_FLIGHT=$(( 640 / shard / py ))
      export PSANA_LEGION_MIN_RUNNING_TASKS=$MAX_TASKS_IN_FLIGHT
      for io in 64 32 16 8; do
        if (( shard * py >= 16 && shard * py <= 32 && shard * io >= 16 && shard * io <= 64 )); then
          if [[ ! -e rax_n${n}_shard${shard}_py${py}_io${io}.log ]]; then
            srun -n $(( n * shard + 1 )) -N $(( n + 1 )) --cpus-per-task $(( 256 / shard )) --cpu_bind cores --distribution=arbitrary --output rax_n${n}_shard${shard}_py${py}_io${io}.log \
              shifter \
                $HOST_PSANA_DIR/psana_legion/scripts/psana_legion.sh \
                  -ll:cpu 0 -ll:py $py -ll:isolate_procs -ll:realm_heap_default -ll:io 1 -ll:concurrent_io $io -ll:csize $(( 48000 / shard )) -ll:rsize 0 -ll:gsize 0 -ll:ib_rsize 0 -lg:window 100
                  # -lg:prof $(( n * shard + 1 )) -lg:prof_logfile $PROFILE_DIR/prof_n${n}_shard${shard}_py${py}_io${io}_%.gz
          fi
        fi
      done
    done
  done
done
