#!/bin/bash
#SBATCH --nodes=1
#SBATCH --time=00:15:00
#SBATCH --partition=regular
#SBATCH --constraint=knl
#SBATCH --mail-type=ALL
#SBATCH --account=ACCOUNT

root_dir="$(dirname "${BASH_SOURCE[0]}")"
source "$root_dir/env.sh"

IMAGE=docker:stanfordlegion/psana-legion:latest

srun -n 1 --ntasks-per-node 1 --cpu_bind none \
  shifter --image=$IMAGE \
    --volume=$HOST_PSANA_DIR:/native-psana-legion \
    --volume=$HOST_LEGION_DIR:/legion \
    --volume=$HOST_DATA_DIR:/reg
    /native-psana-legion/psana_legion/scripts/psana_legion.sh -ll:py 1 -ll:io 1
