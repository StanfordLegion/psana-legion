#!/bin/bash
#SBATCH --job-name=psana_legion
#SBATCH --dependency=singleton
#SBATCH --nodes=1
#SBATCH --time=00:30:00
#SBATCH --partition=debug # regular
#SBATCH --constraint=haswell
#SBATCH --mail-type=ALL
#SBATCH --account=ACCOUNT

# Host directory where data is located
HOST_DATA_DIR=$SCRATCH/data/reg
# HOST_DATA_DIR=$SCRATCH/stripe_24_data/reg
# HOST_DATA_DIR=$SCRATCH/stripe_c24_s16_data/reg

echo "HOST_DATA_DIR=$HOST_DATA_DIR"

filename=$HOST_DATA_DIR/d/psdm/cxi/cxid9114/xtc/e438-r0108-s00-c00.xtc

for n in 1; do
  for c in 1 2 4 8 16 32 64 128 256; do
    for s in 1048576; do
      srun -n $n -N $n --cpus-per-task $(( c * 2 )) --cpu_bind cores --output haswell_multi_n"$n"_c"$c"_s"$s".log \
        ./io_test $filename $c $s

      srun -n $n -N $n --cpus-per-task 2 --cpu_bind cores --output haswell_single_n"$n"_c"$c"_s"$s".log \
        ./io_test $filename $c $s
    done
  done
done
