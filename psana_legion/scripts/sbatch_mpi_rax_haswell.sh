#!/bin/bash
#SBATCH --job-name=psana_legion
#SBATCH --dependency=singleton
#SBATCH --nodes=17
#SBATCH --time=00:30:00
#SBATCH --partition=debug # regular
#SBATCH --constraint=haswell
#SBATCH --image=docker:stanfordlegion/psana-mpi:latest
#SBATCH --exclusive # causes shifter to preload image before run
#SBATCH --mail-type=ALL
#SBATCH --account=ACCOUNT

# Host directory where Psana is located
# (Needed for Python script)
HOST_PSANA_DIR=$HOME/psana_legion/psana-legion

# Host directory where data is located
# HOST_DATA_DIR=$SCRATCH/data/reg
# HOST_DATA_DIR=$SCRATCH/stripe_24_data/reg
# HOST_DATA_DIR=$SCRATCH/stripe_c24_s16_data/reg
HOST_DATA_DIR=$SCRATCH/noepics_data/reg

echo "HOST_DATA_DIR=$HOST_DATA_DIR"

export EAGER=0

for n in 1 2 4 8 16; do
  for c in 32; do
    ./make_nodelist.sh $c
    export SLURM_HOSTFILE=$PWD/nodelist.txt
    if [[ ! -e rax_n"$n"_c"$c".log ]]; then
      srun -n $(( n * c + 1 )) -N $(( n + 1 )) --cpus-per-task 2 --cpu_bind cores --distribution=arbitrary --output rax_n"$n"_c"$c".log \
        shifter \
          --volume=$HOST_PSANA_DIR:/native-psana-legion \
          --volume=$HOST_DATA_DIR:/reg \
          python /native-psana-legion/psana_legion/mpi_rax.py
    fi
  done
done