#!/bin/bash

set -e

root_dir="$(dirname "${BASH_SOURCE[0]}")"
export PYTHONPATH="$PYTHONPATH:$root_dir"
export PS_PARALLEL=legion

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

legion_python user -ll:py 1
