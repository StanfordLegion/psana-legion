#!/bin/bash
#SBATCH --job-name=psana_legion
#SBATCH --dependency=singleton
#SBATCH --nodes=1
#SBATCH --time=00:30:00
#SBATCH --partition=regular
#SBATCH --constraint=knl,quad,cache
#SBATCH --core-spec=4
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
HOST_DATA_DIR=$SCRATCH/data/reg

echo "HOST_DATA_DIR=$HOST_DATA_DIR"

for n in 1; do
  for c in 1 2 4 8 16 21; do
  for i in 1; do
    srun -n $(( n * c )) -N 1 --cpus-per-task $(( 256 / c )) --cpu_bind cores --output rax_n"$n"_c"$c"_i"$i".log \
      shifter --image=$IMAGE \
        --volume=$HOST_PSANA_DIR:/native-psana-legion \
        --volume=$HOST_LEGION_DIR:/legion \
        --volume=$HOST_DATA_DIR:/reg \
        /native-psana-legion/psana_legion/scripts/psana_legion.sh \
	  -ll:py 1 -ll:io $i -ll:csize 24576 -lg:window 20
  done
  done
done
