#!/bin/bash

set -e

root_dir="$(dirname "${BASH_SOURCE[0]}")"
source "$root_dir"/../setup/env.sh

export PYTHONPATH="$PYTHONPATH:$root_dir"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$root_dir/build"
export PS_PARALLEL=legion

export KERNEL_KIND=sum
export LIMIT=10

if [[ ! -d .tmp ]]; then
    echo "The .tmp directory does not exist. Please run:"
    echo
    echo "    pushd ../lcls2 # wherever you checked out lcls2 repo"
    echo "    source setup_env.sh"
    echo "    ./build_all.sh"
    echo "    pytest psana/psana/tests"
    echo "    popd"
    echo "    cp -r ../lcls2/.tmp ."
    false
fi

legion_python user.py -ll:py 1 -ll:cpu 0 -ll:gpu 1
