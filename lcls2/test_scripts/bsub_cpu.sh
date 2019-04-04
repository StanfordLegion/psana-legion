#!/bin/bash
#BSUB -P CHM137
#BSUB -W 0:30
#BSUB -nnodes 1
#BSUB -o lsf-%J.out
#BSUB -e lsf-%J.err
#BSUB -N

root_dir="$PWD"

source "$root_dir"/../setup/env.sh

export PYTHONPATH="$PYTHONPATH:$root_dir"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$root_dir/build"
# uncomment this line when building Legion outside of conda build
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$REL_DIR/lib"
export PS_PARALLEL=legion

# export DATA_DIR=$MEMBERWORK/chm137/mona_small_data
export DATA_DIR=$WORLDWORK/chm137/mona_small_data

export LIMIT=10

export all_proxy=socks://proxy.ccs.ornl.gov:3128/
export ftp_proxy=ftp://proxy.ccs.ornl.gov:3128/
export http_proxy=http://proxy.ccs.ornl.gov:3128/
export https_proxy=https://proxy.ccs.ornl.gov:3128/
export no_proxy='localhost,127.0.0.0/8,*.ccs.ornl.gov,*.ncrc.gov'

nodes=$(( ( LSB_MAX_NUM_PROCESSORS - 1 ) / 42 ))

jsrun -n $(( nodes * 2 )) --rs_per_host 2 --tasks_per_rs 1 --cpu_per_rs 21 --gpu_per_rs 3 --bind rs ./pick_hcas.py legion_python user -ll:py 1 -ll:cpu 1 -ll:force_kthreads 1 # note: keep the argument to force_kthreads, otherwise legion.py can't parse it
