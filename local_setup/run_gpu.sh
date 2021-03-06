#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source ${DIR}/env.sh
# export REALM_FREEZE_ON_ERROR=1
( pushd ${PSANA_LEGION_DIR} && \
PSANA_MAPPER=simple KERNEL_KIND=sum EAGER=1 LIMIT=100 REALM_SYNTHETIC_CORE_MAP= mpirun -n 3 ./psana_legion -ll:py 1 -ll:io 1 -ll:gpu 1 -ll:csize 6000 -lg:window 50 || echo FAILURE $? && \
popd )
