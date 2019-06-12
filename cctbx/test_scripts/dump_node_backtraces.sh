#!/bin/bash

set -x

out_dir="$1"
pstree -u $(whoami) -p > "$out_dir/pstree_$(hostname).log"

i=0
for pid in $(pgrep -u $(whoami) psana_legion); do
    gdb -p $pid -batch -quiet -ex "thread apply all bt" 2>&1 > "$out_dir/bt_$(hostname)_$pid.log" &
    let i++
    if [[ $(( i % 10 )) == 0 ]]; then
        wait
    fi
done
wait
