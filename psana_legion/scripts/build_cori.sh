#!/bin/bash

root_dir="$(dirname "${BASH_SOURCE[0]}")"
source "$root_dir/env.sh"

FORCE_PYTHON=1 PYTHON_LIB=/conda/lib/libpython2.7.so LG_RT_DIR=$HOST_LEGION_DIR/runtime DEBUG=0 make -j16
