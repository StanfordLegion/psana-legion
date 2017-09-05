#!/bin/bash
#SBATCH --nodes=1
#SBATCH --time=00:15:00
#SBATCH --partition=regular
#SBATCH --constraint=knl
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
HOST_DATA_DIR=$HOME/psana_legion/data/reg

srun -n 1 --ntasks-per-node 1 --cpu_bind none \
  shifter --image=$IMAGE \
    --volume=$HOST_PSANA_DIR:/native-psana-legion \
    --volume=$HOST_LEGION_DIR:/legion \
    --volume=$HOST_DATA_DIR:/reg \
    /native-psana-legion/psana_legion/scripts/psana_legion.sh -ll:py 1 -ll:io 1
