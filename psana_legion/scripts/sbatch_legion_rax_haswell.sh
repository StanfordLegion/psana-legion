#!/bin/bash
#SBATCH --job-name=psana_legion
#SBATCH --dependency=singleton
#SBATCH --nodes=17
#SBATCH --time=00:30:00
#SBATCH --partition=debug # regular
#SBATCH --constraint=haswell
#SBATCH --image=docker:stanfordlegion/psana-legion:latest
#SBATCH --exclusive # causes shifter to preload image before run
#SBATCH --mail-type=ALL
#SBATCH --account=lcls

# Host directory where Psana is located
# (Needed for native Legion shared library)
HOST_PSANA_DIR=$HOME/psana_legion/psana-legion

# Host directory where Legion is located
# (Needed for Python bindings)
HOST_LEGION_DIR=$HOME/psana_legion/legion

# Host directory where data is located
# HOST_DATA_DIR=$SCRATCH/data/reg
# HOST_DATA_DIR=$SCRATCH/stripe_24_data/reg
HOST_DATA_DIR=$SCRATCH/noepics_data/reg

export SIT_PSDM_DATA=$HOST_DATA_DIR/d/psdm

echo "HOST_DATA_DIR=$HOST_DATA_DIR"

export EAGER=1
export PSANA_MAPPER=simple

for n in 1 2 4 8 16; do
  for c in 8; do
    ./make_nodelist.py $c > nodelist.txt
    export SLURM_HOSTFILE=$PWD/nodelist.txt
    for i in 8; do
      if [[ ! -e rax_n"$n"_c"$c"_i"$i".log ]]; then
        srun -n $(( n * c + 1 )) -N $(( n + 1 )) --cpus-per-task $(( 64 / c )) --cpu_bind cores --distribution=arbitrary --output rax_n"$n"_c"$c"_i"$i".log \
          shifter \
            $HOST_PSANA_DIR/psana_legion/scripts/psana_legion.sh \
              -ll:py 1 -ll:io 1 -ll:concurrent_io $i -ll:csize 6000 -ll:rsize 0 -ll:gsize 0 -lg:window 100
              # -lg:prof $(( n * c )) -lg:prof_logfile prof_n"$n"_c"$c"_i"$i"_%.gz
      fi
    done
  done
done
