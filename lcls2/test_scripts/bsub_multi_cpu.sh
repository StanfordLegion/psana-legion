#!/bin/bash
#BSUB -P CSC103SUMMITDEV
#BSUB -W 0:30
#BSUB -nnodes 3
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

export all_proxy=socks://proxy.ccs.ornl.gov:3128/
export ftp_proxy=ftp://proxy.ccs.ornl.gov:3128/
export http_proxy=http://proxy.ccs.ornl.gov:3128/
export https_proxy=https://proxy.ccs.ornl.gov:3128/
export no_proxy='localhost,127.0.0.0/8,*.ccs.ornl.gov,*.ncrc.gov'

jsrun -n 3 --rs_per_host 1 --tasks_per_rs 1 legion_python user -ll:py 1 -ll:cpu 1 -level announce=2