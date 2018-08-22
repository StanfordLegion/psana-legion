#!/bin/bash

export PSANA_USE_MPI=1
export AUGMENT_MODULES_PATH=0
root_dir="$(dirname "${BASH_SOURCE[0]}")"
"$root_dir/build_cori.sh"
