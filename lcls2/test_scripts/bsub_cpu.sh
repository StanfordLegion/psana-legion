#!/bin/bash
#BSUB -P CSC103SUMMITDEV
#BSUB -W 0:30
#BSUB -nnodes 1
#BSUB -o lsf-%J.out
#BSUB -e lsf-%J.err
#BSUB -N

root_dir="$PWD"
export PYTHONPATH="$PYTHONPATH:$root_dir"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$root_dir/build"
# uncomment this line when building Legion outside of conda build
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$REL_DIR/lib"
export PS_PARALLEL=legion

export LIMIT=10

jsrun -n 1 legion_python user -ll:py 1 -ll:cpu 1
