#
# Top level Makefile for running with docker
#

DOCKER_IMAGE_DIR=${PSANA_LEGION_DIR}/../docker
DOCKER_IMAGE_SOURCE=${DOCKER_IMAGE_DIR}/Dockerfile.psana-legion
DOCKER_IMAGE_ID_FILE=${PSANA_LEGION_DIR}/../.dockerid
ifeq "$(wildcard ${DOCKER_IMAGE_ID_FILE})" ""
DOCKER_IMAGE_ID=""
else
DOCKER_IMAGE_ID:="$(shell cat ${DOCKER_IMAGE_ID_FILE})"
endif

all:	start

.PHONY:	build
build:	$(wildcard psana_legion/*.cc) $(wildcard psana_legion/*.py)
	cd psana_legion ; FORCE_PYTHON=1 PYTHON_VERSION_MAJOR=2 PYTHON_LIB=/conda/lib/libpython2.7.so make

.PHONY:	run
run:
	LD_LIBRARYPATH=${LD_LIBRARY_PATH}:~/PSANA/psana-legion/psana_legion/
	EAGER=1 REALM_SYNTHETIC_CORE_MAP= mpirun -n 4 -x USE_GASNET=1 -x LD_LIBRARY_PATH ~/PSANA/psana-legion/psana_legion/psana_legion -ll:py 0 -ll:io 2 -ll:csize 6000 -lg:window 50 -level task_pool_mapper=1

.PHONY:	docker_image
${DOCKER_IMAGE_ID_FILE}:	${DOCKER_IMAGE_SOURCE}
	@echo === Building docker image ===
	docker build -f ${DOCKER_IMAGE_SOURCE} --iidfile ${DOCKER_IMAGE_ID_FILE} .

docker_image_id:	${DOCKER_IMAGE_ID_FILE}
	echo DOCKER_IMAGE_ID ${DOCKER_IMAGE_ID}

start:	docker_image_id
	@make start_

.PHONY:	start_
start_:
	@echo === Starting docker image ${DOCKER_IMAGE_ID} ===
	docker run -ti --volume=${SIT_PSDM_DATA}:/reg/d/psdm --volume=${PSANA_LEGION_DIR}/..:/psana-legion-dev --volume=${LG_RT_DIR}/.. ${DOCKER_IMAGE_ID}

.PHONY:	clean
clean:
	rm -f ${DOCKER_IMAGE_ID_FILE}
