#!/bin/bash
#SBATCH --job-name=psana_legion
#SBATCH --dependency=singleton
#SBATCH --nodes=65
#SBATCH --time=03:00:00
#SBATCH --qos=regular
#SBATCH --constraint=knl,quad,cache
#SBATCH --core-spec=4
#SBATCH --image=docker:stanfordlegion/psana-legion:elliott
#SBATCH --exclusive # causes shifter to preload image before run
#SBATCH --mail-type=ALL
#SBATCH --account=m2859

# Host directory where Psana is located
# (Needed for native Legion shared library)
HOST_PSANA_DIR=$HOME/psana_legion/psana-legion

# Host directory where Legion is located
# (Needed for Python bindings)
HOST_LEGION_DIR=$HOME/psana_legion/legion

# Host directory where data is located
# HOST_DATA_DIR=$SCRATCH/data/reg
# HOST_DATA_DIR=$SCRATCH/stripe_24_data/reg
HOST_DATA_DIR=$SCRATCH/noepics_c24_s1_data/reg

export SIT_PSDM_DATA=$HOST_DATA_DIR/d/psdm

echo "HOST_DATA_DIR=$HOST_DATA_DIR"

export EAGER=1
export PSANA_MAPPER=simple
export KERNEL_KIND=memory_bound

export GASNET_PHYSMEM_MAX=64G # per node

for rounds in 80; do
  export KERNEL_ROUNDS=$rounds
  for n in 64; do
    export LIMIT=$(( n * 2048 ))
    for c in 32 21 16 8 4; do
      ./make_nodelist.py $c > nodelist.txt
      export SLURM_HOSTFILE=$PWD/nodelist.txt
      export MAX_TASKS_IN_FLIGHT=$(( 1280 / c ))
      for i in 1 2 4 8; do
        if [[ ! -e rax_rounds"$rounds"_n"$n"_c"$c"_i"$i".log ]]; then
          srun -n $(( n * c + 1 )) -N $(( n + 1 )) --cpus-per-task $(( 256 / c )) --cpu_bind cores --distribution=arbitrary --output rax_rounds"$rounds"_n"$n"_c"$c"_i"$i".log \
            shifter \
              $HOST_PSANA_DIR/psana_legion/scripts/psana_legion.sh \
                -ll:cpu 0 -ll:py 1 -ll:io 1 -ll:concurrent_io $i -ll:csize $(( 48000 / c )) -ll:rsize 0 -ll:gsize 0 -ll:ib_rsize 0 -lg:window 100
                # -lg:prof $(( n * c + 1 )) -lg:prof_logfile prof_rounds"$rounds"_n"$n"_c"$c"_i"$i"_%.gz
        fi
      done
    done
  done
done
