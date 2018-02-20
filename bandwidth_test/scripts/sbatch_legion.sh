#!/bin/bash
#SBATCH --nodes=2
#SBATCH --time=00:30:00
#SBATCH --qos=debug
#SBATCH --constraint=knl,quad,cache
#SBATCH --core-spec=4
#SBATCH --mail-type=ALL
#SBATCH --account=lcls

for rounds in 20 40 80; do
  for n in 2 1; do
    for c in 128 64 32 16 8 4; do
      for p in 1 2 4 8 16 32; do
        if (( $p * $c >= 64 && $p * $c <= 128 )); then
          if [[ ! -e legion_rounds"$rounds"_n"$n"_c"$c"_p"$p".log ]]; then
            srun -n $(( n * c )) -N $n --cpus-per-task $(( 256 / c )) --cpu_bind cores --output legion_rounds"$rounds"_n"$n"_c"$c"_p"$p".log \
              ./test_legion -rounds $rounds -size 64 -iter 100 \
                -ll:cpu $p -ll:csize 1 -ll:rsize 0 -ll:gsize 0
                # -lg:prof $(( n * c )) -lg:prof_logfile prof_rounds"$rounds"_n"$n"_c"$c"_p"$p"_%.gz
          fi
        fi
      done
    done
  done
done
