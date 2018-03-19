DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source ${DIR}/env.sh
( pushd ${DIR}/../psana_legion && \
REALM_FREEZE_ON_ERROR=1 EAGER=1 REALM_SYNTHETIC_CORE_MAP= mpirun -n 3 ./psana_legion -ll:py 1 -ll:io 1 -ll:csize 6000 -lg:window 50 || echo FAILURE $? && \
popd )
