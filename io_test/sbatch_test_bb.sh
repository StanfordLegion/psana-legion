#!/bin/bash
#SBATCH --nodes=1
#SBATCH --time=00:30:00
#SBATCH --partition=debug # regular
#SBATCH --constraint=knl,quad,cache
#SBATCH --core-spec=4
#SBATCH --mail-type=ALL
#SBATCH --account=ACCOUNT
#DW jobdw capacity=30GB access_mode=striped type=scratch
#DW stage_in source=/global/cscratch1/sd/slaughte/data/reg/d/psdm/cxi/cxid9114/xtc/e438-r0108-s00-c00.xtc destination=$DW_JOB_STRIPED/e438-r0108-s00-c00.xtc type=file

filename=$DW_JOB_STRIPED/e438-r0108-s00-c00.xtc

for n in 1; do
  for c in 1 2 4 8 16 32 64 128 256; do
    for s in 1048576; do
      srun -n $n -N $n --cpus-per-task $(( c * 4 )) --cpu_bind cores --output knl_multi_n"$n"_c"$c"_s"$s".log \
        ./io_test $filename $c $s

      srun -n $n -N $n --cpus-per-task 4 --cpu_bind cores --output knl_single_n"$n"_c"$c"_s"$s".log \
        ./io_test $filename $c $s
    done
  done
done
