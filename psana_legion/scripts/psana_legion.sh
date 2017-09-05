#!/bin/bash

root_dir="$(dirname "${BASH_SOURCE[0]}")"
psana_dir="$(dirname "$root_dir")"
export LD_LIBRARY_PATH="$psana_dir:$psana_dir/lib64:$LD_LIBRARY_PATH"
"/psana-legion/psana_legion/psana_legion" -ll:py 1 -ll:io 1 -ll:csize 8192
