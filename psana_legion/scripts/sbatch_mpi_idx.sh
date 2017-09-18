#!/bin/bash
#SBATCH --job-name=psana_legion
#SBATCH --dependency=singleton
#SBATCH --nodes=4
#SBATCH --time=00:30:00
#SBATCH --partition=debug # regular
#SBATCH --constraint=knl,quad,cache
#SBATCH --core-spec=4
#SBATCH --mail-type=ALL
#SBATCH --account=ACCOUNT

IMAGE=docker:stanfordlegion/psana-mpi:latest

# Host directory where Psana is located
# (Needed for Python script)
HOST_PSANA_DIR=$HOME/psana_legion/psana-legion

# Host directory where data is located
HOST_DATA_DIR=$SCRATCH/data/reg
# HOST_DATA_DIR=$SCRATCH/stripe_24_data/reg

echo "HOST_DATA_DIR=$HOST_DATA_DIR"

for n in 4 2 1; do
  for c in 1 2 4 8 16 32 64; do
    srun -n $(( n * c )) -N $n --cpus-per-task 4 --cpu_bind cores --output idx_n"$n"_c"$c".log \
      shifter --image=$IMAGE \
        --volume=$HOST_PSANA_DIR:/native-psana-legion \
        --volume=$HOST_DATA_DIR:/reg \
        python /native-psana-legion/psana_legion/mpi_idx.py
  done

  for c in 128 256; do
    srun -n $(( n * c )) -N $n --cpus-per-task $(( 256/c )) --cpu_bind cores --output idx_n"$n"_c"$c".log \
      shifter --image=$IMAGE \
        --volume=$HOST_PSANA_DIR:/native-psana-legion \
        --volume=$HOST_DATA_DIR:/reg \
        python /native-psana-legion/psana_legion/mpi_idx.py
  done
done