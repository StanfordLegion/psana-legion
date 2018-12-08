#!/bin/bash
#SBATCH --job-name=psana_legion
#SBATCH --dependency=singleton
#SBATCH --time=00:30:00
#SBATCH --qos=debug
#SBATCH --constraint=knl,quad,cache
#SBATCH --core-spec=4
#SBATCH --image=docker:stanfordlegion/psana-mpi:latest
#SBATCH --exclusive # causes shifter to preload image before run
#SBATCH --mail-type=ALL
#SBATCH --account=m2859

# Host directory where Psana is located
# (Needed for Python script)
HOST_PSANA_DIR=$HOME/psana_legion/psana-legion

# Host directory where data is located
HOST_DATA_DIR=$SCRATCH/data/reg

export SIT_PSDM_DATA=$HOST_DATA_DIR/d/psdm

echo "HOST_DATA_DIR=$HOST_DATA_DIR"

export EAGER=1
export REPEAT=1

for n in $(( SLURM_JOB_NUM_NODES - 1 )); do
  export LIMIT=$(( n * 4096 ))
  for shard in 256 128 64 32; do
    ./make_nodelist.py $shard > nodelist.txt
    export SLURM_HOSTFILE=$PWD/nodelist.txt
    if [[ ! -e rax_n_${n}_shard_${shard}.log ]]; then
      srun -n $(( n * shard + 1 )) -N $(( n + 1 )) --cpus-per-task $(( 256 / shard )) --cpu_bind threads --distribution=arbitrary --output rax_n_${n}_shard_${shard}.log \
        shifter \
          $HOST_PSANA_DIR/psana_legion/scripts/psana_mpi.sh $HOST_PSANA_DIR/psana_legion/mpi_rax.py
    fi
  done
done
