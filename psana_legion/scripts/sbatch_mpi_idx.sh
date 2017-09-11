#!/bin/bash
#SBATCH --job-name=psana_legion
#SBATCH --dependency=singleton
#SBATCH --nodes=1
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

echo "Running MPI in IDX mode..."

echo "HOST_DATA_DIR=$HOST_DATA_DIR"

srun -n 16 -N 1 --cpus-per-task 4 --cpu_bind cores \
  shifter --image=$IMAGE \
    --volume=$HOST_PSANA_DIR:/native-psana-legion \
    --volume=$HOST_DATA_DIR:/reg \
    python /native-psana-legion/psana_legion/mpi_idx.py
