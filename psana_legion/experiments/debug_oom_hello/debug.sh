#!/bin/bash
#SBATCH --nodes=16
#SBATCH --time=00:30:00
#SBATCH --qos=debug
#SBATCH --constraint=knl,quad,cache
#SBATCH --core-spec=4
#SBATCH --mail-type=ALL

export GASNET_PHYSMEM_MAX=4G

n=16
c=32

srun -n $(( n * c )) -N $(( n )) --cpus-per-task $(( 256 / c )) --cpu_bind cores ./hello_world -ll:cpu 1 -ll:rsize 0 -ll:gsize 0 -level activemsg=2 -logfile activemsg_%.log
