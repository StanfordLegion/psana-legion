#!/bin/bash
#SBATCH --job-name=psana_legion
#SBATCH --dependency=singleton
#SBATCH --nodes=16
#SBATCH --time=00:30:00
#SBATCH --partition=debug # regular
#SBATCH --constraint=haswell
#SBATCH --mail-type=ALL
#SBATCH --account=ACCOUNT

IMAGE=docker:stanfordlegion/psana-legion:latest

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

echo "HOST_DATA_DIR=$HOST_DATA_DIR"

export EAGER=0

for n in 1 2 4 8 16; do
  for c in 8; do
    for i in 8; do
      if [[ ! -e rax_n"$n"_c"$c"_i"$i".log ]]; then
        srun -n $(( n * c )) -N $n --cpus-per-task $(( 64 / c )) --cpu_bind cores --output rax_n"$n"_c"$c"_i"$i".log \
          shifter --image=$IMAGE \
            --volume=$HOST_PSANA_DIR:/native-psana-legion \
            --volume=$HOST_LEGION_DIR:/legion \
            --volume=$HOST_DATA_DIR:/reg \
            --volume=$PWD:/output \
            /native-psana-legion/psana_legion/scripts/psana_legion.sh \
              -ll:py 1 -ll:io $i -ll:csize 12000 -lg:window 20
              # -lg:prof $(( n * c )) -lg:prof_logfile /output/prof_n"$n"_c"$c"_i"$i"_%.gz
      fi
    done
  done
done
