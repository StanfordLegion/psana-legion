#!/bin/bash
#SBATCH --nodes=1
#SBATCH --time=00:15:00
#SBATCH --partition=regular
#SBATCH --constraint=knl
#SBATCH --mail-type=ALL
#SBATCH --account=ACCOUNT

# results in 1 rank bound to all cores
srun -n 1 -N 1 --tasks-per-node 1 --cpu_bind none --output="n1_N1_tpn1_none_%N_%t.log" bash -c 'cat /proc/self/status'

# results in 4 ranks bound to 1 core each (4 hyperthreads)
srun -n 4 -N 1 --tasks-per-node 4 --cpu_bind cores --output="n4_N1_tpn4_cores_%N_%t.log" bash -c 'cat /proc/self/status'

# results in 4 ranks bound to 17 cores each (68 hyperthreads)
srun -n 4 -N 1 --cpus-per-task 68 --cpu_bind cores --output="n4_N1_cpt68_cores_%N_%t.log" bash -c 'cat /proc/self/status'
