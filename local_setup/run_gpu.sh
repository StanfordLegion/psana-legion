#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source ${DIR}/env.sh
# export REALM_FREEZE_ON_ERROR=1
( pushd ${PSANA_LEGION_DIR} && \
GASNET_BBACKTRACE=1 REALM_BACKTRACE=1 LEGION_BACKTRACE=1 PSANA_MAPPER=lifeline KERNEL_KIND=sum EAGER=1 LIMIT=100 REALM_SYNTHETIC_CORE_MAP= mpirun -n 3 ./psana_legion -ll:py 1 -ll:io 1 -ll:cpu 0 -ll:gpu 1 -ll:fsize 1024 -ll:csize 6000 -lg:window 50 || echo FAILURE $? && \
popd )
