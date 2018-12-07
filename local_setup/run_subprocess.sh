#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source ${DIR}/env.sh
# export REALM_FREEZE_ON_ERROR=1
# -hl:prof 3 -hl:prof_logfile "${DIR}"/prof_%.gz
( pushd ${PSANA_LEGION_DIR} && \
GASNET_FREEZE_ON_ERROR=1 PSANA_MAPPER=simple EAGER=1 LIMIT=100 REALM_SYNTHETIC_CORE_MAP= mpirun -n 3 ./psana_legion -ll:cpu 0 -ll:py 3 -ll:isolate_procs -ll:realm_heap_default -ll:io 1 -ll:csize 6000 -lg:window 50 || echo FAILURE $? && \
popd )
