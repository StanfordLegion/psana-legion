#!/bin/bash

set -e

root_dir="$(dirname "${BASH_SOURCE[0]}")"
source "$root_dir"/../setup/env.sh

export PYTHONPATH="$PYTHONPATH:$root_dir"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$root_dir/build"
export PS_PARALLEL=legion

export KERNEL_KIND=sum
export LIMIT=10

export DATA_DIR="${DATA_DIR:-/reg/d/psdm/xpp/xpptut15/scratch/mona/xtc2}"
if [[ ! -d $DATA_DIR ]]; then
    echo "DATA_DIR is not set or does not exist. Please check it and rerun."
    false
fi

legion_python user.py -ll:py 1 -ll:cpu 1 -level announce=2
