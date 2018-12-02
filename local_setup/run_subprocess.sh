#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source ${DIR}/env.sh
# export REALM_FREEZE_ON_ERROR=1
( pushd ${PSANA_LEGION_DIR} && \
PSANA_MAPPER=simple EAGER=1 LIMIT=100 REALM_SYNTHETIC_CORE_MAP= mpirun -n 3 ./psana_legion -ll:py 2 -ll:isolate_procs -ll:realm_heap_default -ll:io 1 -ll:csize 6000 -lg:window 50 || echo FAILURE $? && \
popd )
