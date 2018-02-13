OUTPUT_DIR=${PWD}/${PSANA_MAPPER}_mapper_output
mkdir -p ${OUTPUT_DIR}
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
( pushd ${DIR}/../psana_legion && \
EAGER=1 REALM_SYNTHETIC_CORE_MAP= mpiexec -n 4 -x USE_GASNET=1 -x LD_LIBRARY_PATH=${LD_LIBRARY_PATH} ./psana_legion -ll:py 1 -ll:io 1 -ll:csize 6000 -lg:window 50 -level lifeline_mapper=1,task_pool_mapper=1 -logfile ${OUTPUT_DIR}/${PSANA_MAPPER}_%.log || echo FAILURE $? && \
popd )
