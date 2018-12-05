out_dir="$1"
pstree -u $(whoami) -p > "$out_dir/pstree_$(hostname).log"
for pid in $(pgrep -u $(whoami) psana_legion); do
    sudo gdb -p $pid -batch -quiet -ex "thread apply all bt" 2>&1 > "$out_dir/bt_$(hostname)_$pid.log"
done
