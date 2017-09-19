#!/bin/bash
#SBATCH --job-name=psana_legion_bb
#SBATCH --dependency=singleton
#SBATCH --nodes=16
#SBATCH --time=00:30:00
#SBATCH --partition=debug # regular
#SBATCH --constraint=haswell
#SBATCH --image=docker:stanfordlegion/psana-mpi:latest
#SBATCH --exclusive # causes shifter to preload image before run
#SBATCH --mail-type=ALL
#SBATCH --account=ACCOUNT
#DW persistentdw name=slaughte_data_noepics

# Host directory where Psana is located
# (Needed for Python script)
HOST_PSANA_DIR=$HOME/psana_legion/psana-legion

# Host directory where data is located
HOST_DATA_DIR=$DW_PERSISTENT_STRIPED_slaughte_data_noepics/reg

echo "HOST_DATA_DIR=$HOST_DATA_DIR"

export EAGER=0

for n in 1 2 4 8 16; do
  for c in 32; do
    if [[ ! -e rax_n"$n"_c"$c".log ]]; then
      srun -n $(( n * c )) -N $n --cpus-per-task 2 --cpu_bind cores --output rax_n"$n"_c"$c".log \
        shifter \
          --volume=$HOST_PSANA_DIR:/native-psana-legion \
          --volume=$HOST_DATA_DIR:/reg \
          python /native-psana-legion/psana_legion/mpi_rax.py
    fi
  done
done
