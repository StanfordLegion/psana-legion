DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
( pushd ${DIR}/../psana_legion && \
EAGER=1 REALM_SYNTHETIC_CORE_MAP= mpiexec -n 3 -x USE_GASNET=1 -x LD_LIBRARY_PATH=${LD_LIBRARY_PATH} ./psana_legion -ll:py 1 -ll:io 1 -ll:csize 6000 -lg:window 50 -level lifeline_mapper=1 && \
popd )
