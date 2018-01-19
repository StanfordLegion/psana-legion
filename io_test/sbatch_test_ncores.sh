#!/bin/bash
#SBATCH --job-name=psana_legion
#SBATCH --dependency=singleton
#SBATCH --nodes=1
#SBATCH --time=01:00:00
#SBATCH --partition=regular
#SBATCH --constraint=knl,quad,cache
#SBATCH --core-spec=4
#SBATCH --mail-type=ALL
#SBATCH --account=ACCOUNT

# Host directory where data is located
HOST_DATA_DIR=$SCRATCH/data/reg
# HOST_DATA_DIR=$SCRATCH/stripe_24_data/reg
# HOST_DATA_DIR=$SCRATCH/stripe_c24_s16_data/reg

echo "HOST_DATA_DIR=$HOST_DATA_DIR"

filename=$HOST_DATA_DIR/d/psdm/cxi/cxid9114/xtc/e438-r0108-s00-c00.xtc

for n in 1; do
  for p in 1 2 4 8 16 32 64; do
    for c in 2 4 8 16 32 64 128 256; do
      if (( c >= p )); then
        for s in 1048576; do
	  if [ ! -e knl_n"$n"_p"$p"_c"$c"_s"$s".log ]; then
	    srun -n $n -N $n --cpus-per-task $(( p * 4 )) --cpu_bind cores --output knl_n"$n"_p"$p"_c"$c"_s"$s".log \
	      ./io_test $filename $c $s
	  fi
	done
      fi
    done
  done
done
