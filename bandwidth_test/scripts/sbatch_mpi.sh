#!/bin/bash
#SBATCH --nodes=3
#SBATCH --time=00:30:00
#SBATCH --qos=debug
#SBATCH --constraint=knl,quad,cache
#SBATCH --core-spec=4
#SBATCH --mail-type=ALL
#SBATCH --account=lcls

for rounds in 20 40 80; do
  for n in 2 1; do
    for c in 128 64 32 16 8 4; do
      if [[ ! -e mpi_rounds"$rounds"_n"$n"_c"$c".log ]]; then
        srun -n $(( n * c )) -N $n --cpus-per-task $(( 256 / c )) --cpu_bind cores --output mpi_rounds"$rounds"_n"$n"_c"$c".log \
          ./test_mpi -rounds $rounds -size 64 -iter 100
      fi
    done
  done
done
