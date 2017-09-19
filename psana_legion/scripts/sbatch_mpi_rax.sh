#!/bin/bash
#SBATCH --job-name=psana_legion
#SBATCH --dependency=singleton
#SBATCH --nodes=16
#SBATCH --time=00:30:00
#SBATCH --partition=debug # regular
#SBATCH --constraint=knl,quad,cache
#SBATCH --core-spec=4
#SBATCH --mail-type=ALL
#SBATCH --account=ACCOUNT

IMAGE=docker:stanfordlegion/psana-mpi:latest

# Host directory where Psana is located
# (Needed for Python script)
HOST_PSANA_DIR=$HOME/psana_legion/psana-legion

# Host directory where data is located
HOST_DATA_DIR=$SCRATCH/data/reg
# HOST_DATA_DIR=$SCRATCH/stripe_24_data/reg
# HOST_DATA_DIR=$SCRATCH/stripe_c24_s16_data/reg

echo "HOST_DATA_DIR=$HOST_DATA_DIR"

export EAGER=0

for n in 1 2 4 8 16; do
  for c in 64; do # 32 16 8 4 2 1
    srun -n $(( n * c )) -N $n --cpus-per-task 4 --cpu_bind cores --output rax_n"$n"_c"$c".log \
      shifter --image=$IMAGE \
        --volume=$HOST_PSANA_DIR:/native-psana-legion \
        --volume=$HOST_DATA_DIR:/reg \
        python /native-psana-legion/psana_legion/mpi_rax.py
  done

  # for c in 128 256; do
  #   srun -n $(( n * c )) -N $n --cpus-per-task $(( 256/c )) --cpu_bind threads --output rax_n"$n"_c"$c".log \
  #     shifter --image=$IMAGE \
  #       --volume=$HOST_PSANA_DIR:/native-psana-legion \
  #       --volume=$HOST_DATA_DIR:/reg \
  #       python /native-psana-legion/psana_legion/mpi_rax.py
  # done
done
