#!/bin/bash

set -x

root_dir="$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"

# job="$1"
scratch_dir="$1"
mkdir -p "$scratch_dir"

i=0

hosts=$(scontrol show hostname $SLURM_JOB_NODELIST)
# hosts=$(scontrol show hostname $(scontrol show job $job | grep ' NodeList' | cut -d= -f2))
for host in $hosts; do
    ssh $host shifter --image=docker:stanfordlegion/cctbx-legion:subprocess-2019-06-debug bash "$root_dir/dump_node_backtraces.sh" "$scratch_dir" &
    let i++
    if [[ $(( i % 200 )) == 0 ]]; then
        wait
    fi
done
wait
