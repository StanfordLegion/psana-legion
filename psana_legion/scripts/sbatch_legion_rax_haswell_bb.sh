#!/bin/bash
#SBATCH --job-name=psana_legion_bb
#SBATCH --dependency=singleton
#SBATCH --nodes=17
#SBATCH --time=00:30:00
#SBATCH --partition=debug # regular
#SBATCH --constraint=haswell
#SBATCH --image=docker:stanfordlegion/psana-legion:latest
#SBATCH --exclusive # causes shifter to preload image before run
#SBATCH --mail-type=ALL
#SBATCH --account=ACCOUNT
#DW persistentdw name=slaughte_data_noepics

# Host directory where Psana is located
# (Needed for native Legion shared library)
HOST_PSANA_DIR=$HOME/psana_legion/psana-legion

# Host directory where Legion is located
# (Needed for Python bindings)
HOST_LEGION_DIR=$HOME/psana_legion/legion

# Host directory where data is located
HOST_DATA_DIR=$DW_PERSISTENT_STRIPED_slaughte_data_noepics/reg

echo "HOST_DATA_DIR=$HOST_DATA_DIR"

export EAGER=0

for n in 1 2 4 8 16; do
  for c in 8; do
    ./make_nodelist.py $c > nodelist.txt
    export SLURM_HOSTFILE=$PWD/nodelist.txt
    for i in 8; do
      if [[ ! -e rax_n"$n"_c"$c"_i"$i".log ]]; then
        srun -n $(( n * c + 1 )) -N $(( n + 1 )) --cpus-per-task $(( 64 / c )) --cpu_bind cores --distribution=arbitrary --output rax_n"$n"_c"$c"_i"$i".log \
          shifter \
            --volume=$HOST_PSANA_DIR:/native-psana-legion \
            --volume=$HOST_LEGION_DIR:/legion \
            --volume=$HOST_DATA_DIR:/reg \
            --volume=$PWD:/output \
            /native-psana-legion/psana_legion/scripts/psana_legion.sh \
              -ll:py 1 -ll:io $i -ll:csize 12000
              # -lg:prof $(( n * c )) -lg:prof_logfile /output/prof_n"$n"_c"$c"_i"$i"_%.gz
              # -lg:window 20
      fi
    done
  done
done
