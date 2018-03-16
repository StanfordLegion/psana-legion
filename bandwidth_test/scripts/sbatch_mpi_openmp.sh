#!/bin/bash
#SBATCH --nodes=1
#SBATCH --time=00:30:00
#SBATCH --qos=debug
#SBATCH --constraint=knl,quad,cache
#SBATCH --core-spec=4
#SBATCH --mail-type=ALL
#SBATCH --account=lcls

for rounds in 20 80; do
  for n in 1; do
    for c in 128 64 32 16 8 4; do
      for p in 1 2 4 8 16 32; do
        export OMP_NUM_THREADS=$p
        if (( $p * $c >= 64 && $p * $c <= 128 )); then
          if [[ ! -e mpi_rounds"$rounds"_n"$n"_c"$c".log ]]; then
            srun -n $(( n * c )) -N $n --cpus-per-task $(( 256 / c )) --cpu_bind cores --output mpi_openmp_rounds"$rounds"_n"$n"_c"$c".log \
              ./test_mpi_openmp -rounds $rounds -size 64 -iter 100
          fi
        fi
      done
    done
  done
done
