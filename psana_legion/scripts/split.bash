#!/bin/bash
echo splitting into multiple files
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
egrep "node 1d0000 proc .*:" run.log > run0.log
egrep "node 1d0001 proc .*:" run.log > run1.log
egrep "node 1d0002 proc .*:" run.log > run2.log
echo extracting task times
for f in run*log ; do source ${DIR}/extractTimes.bash $f ; done
