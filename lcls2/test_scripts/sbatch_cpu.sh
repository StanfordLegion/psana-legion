#!/bin/bash
#SBATCH --nodes=1
#SBATCH --time=00:30:00
#SBATCH --qos=debug # regular
#SBATCH --constraint=knl,quad,cache
#SBATCH --core-spec=4
#SBATCH --mail-type=ALL
#SBATCH --account=m2859

root_dir="$PWD"

source "$root_dir"/../setup/env.sh

export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$root_dir/build"
export PS_PARALLEL=legion

export DATA_DIR=$SCRATCH/mona_small_data

export LIMIT=10

nodes=$SLURM_JOB_NUM_NODES

srun -n $nodes -N $nodes --ntasks-per-node 1 --cpu_bind none legion_python user.py -ll:py 1 -ll:cpu 1
