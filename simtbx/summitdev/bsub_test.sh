#!/bin/bash
#BSUB -P CSC103SUMMITDEV
#BSUB -W 0:30
#BSUB -nnodes 1
#BSUB -o lsf-%J.out
#BSUB -e lsf-%J.err
#BSUB -N

jsrun -n 1 -g 1 bash test.sh
