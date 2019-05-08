#!/bin/bash
#BSUB -P CSC103SUMMITDEV
#BSUB -W 0:30
#BSUB -nnodes 1
#BSUB -o lsf-%J.out
#BSUB -e lsf-%J.err
#BSUB -N

root_dir="$PWD"

source "$root_dir"/../setup/env.sh

export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$root_dir/build"
export PS_PARALLEL=legion

export DATA_DIR=$MEMBERWORK/chm137/mona_small_data

export LIMIT=10

export IBV_FORK_SAFE=1 # workaround for https://upc-bugs.lbl.gov/bugzilla/show_bug.cgi?id=3908

export all_proxy=socks://proxy.ccs.ornl.gov:3128/
export ftp_proxy=ftp://proxy.ccs.ornl.gov:3128/
export http_proxy=http://proxy.ccs.ornl.gov:3128/
export https_proxy=https://proxy.ccs.ornl.gov:3128/
export no_proxy='localhost,127.0.0.0/8,*.ccs.ornl.gov,*.ncrc.gov'

nodes=$(( ( LSB_MAX_NUM_PROCESSORS - 1 ) / 42 ))

jsrun -n $(( nodes * 2 )) --rs_per_host 2 --tasks_per_rs 1 --cpu_per_rs 21 --gpu_per_rs 3 --bind rs --smpiargs="-x PAMI_DISABLE_CUDA_HOOK=1 -disable_gpu_hooks" ./pick_hcas.py legion_python user.py -ll:py 1 -ll:cpu 0 -ll:gpu 1
