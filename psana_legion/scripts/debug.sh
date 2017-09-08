#!/bin/bash
#SBATCH --nodes=1
#SBATCH --time=00:15:00
#SBATCH --partition=regular
#SBATCH --constraint=knl,quad,flat
#SBATCH --core-spec=4 # reserve 4 cores for the OS
#SBATCH --mail-type=ALL
#SBATCH --account=ACCOUNT

# Meaning of --constraint:
# flat vs cache: use MCDRAM as a scratchpad vs a cache
# quad vs snc2 vs snc4: in flat mode, expose 1, 2, or 4 NUMA domains

# results in 1 rank bound to all cores
srun -n 1 -N 1 --tasks-per-node 1 --cpu_bind none --output="bind_n1_N1_tpn1_none_%N_%t.log" bash -c 'cat /proc/self/status'

# results in 4 ranks bound to 1 core each (4 hyperthreads)
srun -n 4 -N 1 --tasks-per-node 4 --cpu_bind cores --output="bind_n4_N1_tpn4_cores_%N_%t.log" bash -c 'cat /proc/self/status'

# results in 4 ranks bound to 16 cores each (64 hyperthreads)
srun -n 4 -N 1 --cpus-per-task 64 --cpu_bind cores --output="bind_n4_N1_cpt64_cores_%N_%t.log" bash -c 'cat /proc/self/status'

# same with NUMA bound to MCDRAM
srun -n 4 -N 1 --cpus-per-task 64 --cpu_bind cores --output="numa_n4_N1_cpt64_cores_%N_%t.log" numactl --show
