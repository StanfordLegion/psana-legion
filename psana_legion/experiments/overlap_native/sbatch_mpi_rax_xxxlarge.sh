#!/bin/bash
#SBATCH --job-name=psana_legion
#SBATCH --dependency=singleton
#SBATCH --nodes=129
#SBATCH --time=01:00:00
#SBATCH --qos=regular
#SBATCH --constraint=knl,quad,cache
#SBATCH --core-spec=4
#SBATCH --image=docker:stanfordlegion/psana-mpi:latest
#SBATCH --exclusive # causes shifter to preload image before run
#SBATCH --mail-type=ALL
#SBATCH --account=m2859

# Host directory where Psana is located
# (Needed for Python script)
HOST_PSANA_DIR=$HOME/psana_legion/psana-legion

# Host directory where data is located
# HOST_DATA_DIR=$SCRATCH/data/reg
# HOST_DATA_DIR=$SCRATCH/stripe_24_data/reg
# HOST_DATA_DIR=$SCRATCH/stripe_c24_s16_data/reg
HOST_DATA_DIR=$SCRATCH/noepics_c24_s1_data/reg

export SIT_PSDM_DATA=$HOST_DATA_DIR/d/psdm

echo "HOST_DATA_DIR=$HOST_DATA_DIR"

export EAGER=1

export KERNEL_KIND=memory_bound_native

for rounds in 20 40 80 160; do
  export KERNEL_ROUNDS=$rounds
  for n in 128; do
    export LIMIT=$(( n * 2048 ))
    for c in 256 128 64 32 16; do
      ./make_nodelist.py $c > nodelist.txt
      export SLURM_HOSTFILE=$PWD/nodelist.txt
      if [[ ! -e rax_rounds"$rounds"_n"$n"_c"$c".log ]]; then
        srun -n $(( n * c + 1 )) -N $(( n + 1 )) --cpus-per-task $(( 256 / c )) --cpu_bind threads --distribution=arbitrary --output rax_rounds"$rounds"_n"$n"_c"$c".log \
          shifter \
            $HOST_PSANA_DIR/psana_legion/scripts/psana_mpi.sh $HOST_PSANA_DIR/psana_legion/mpi_rax.py
      fi
    done
  done
done
