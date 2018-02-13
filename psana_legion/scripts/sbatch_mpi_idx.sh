#!/bin/bash
#SBATCH --job-name=psana_legion
#SBATCH --dependency=singleton
#SBATCH --nodes=16
#SBATCH --time=00:30:00
#SBATCH --qos=debug # regular
#SBATCH --constraint=knl,quad,cache
#SBATCH --core-spec=4
#SBATCH --image=docker:stanfordlegion/psana-mpi:latest
#SBATCH --exclusive # causes shifter to preload image before run
#SBATCH --mail-type=ALL
#SBATCH --account=lcls

# Host directory where Psana is located
# (Needed for Python script)
HOST_PSANA_DIR=$HOME/psana_legion/psana-legion

# Host directory where data is located
HOST_DATA_DIR=$SCRATCH/data/reg
# HOST_DATA_DIR=$SCRATCH/stripe_24_data/reg
# HOST_DATA_DIR=$SCRATCH/stripe_c24_s16_data/reg

export SIT_PSDM_DATA=$HOST_DATA_DIR/d/psdm

echo "HOST_DATA_DIR=$HOST_DATA_DIR"

for n in 1 2 4 8 16; do
  for c in 128 64; do # 32 16 8 4 2 1
    srun -n $(( n * c )) -N $n --cpus-per-task $(( 256 / c )) --cpu_bind cores --output idx_n"$n"_c"$c".log \
      shifter \
        python $HOST_PSANA_DIR/psana_legion/mpi_idx.py
  done
done
