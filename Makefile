#
# Top level Makefile for running with docker
#

DOCKER_IMAGE_DIR=${PSANA_LEGION_DIR}/../docker
DOCKER_IMAGE_SOURCE=${DOCKER_IMAGE_DIR}/Dockerfile.psana-legion
DOCKER_IMAGE_ID_FILE=${PSANA_LEGION_DIR}/../.dockerid
DOCKER_IMAGE_ID:="$(shell cat ${DOCKER_IMAGE_ID_FILE})"

all:	start

.PHONY:	docker_image
${DOCKER_IMAGE_ID_FILE}:	${DOCKER_IMAGE_SOURCE}
	docker build -f ${DOCKER_IMAGE_SOURCE} --iidfile ${DOCKER_IMAGE_ID_FILE} .

docker_image_id:	${DOCKER_IMAGE_ID_FILE}
	echo DOCKER_IMAGE_ID ${DOCKER_IMAGE_ID}

start:	docker_image_id
	@make start_

.PHONY:	.start_
start_:
	docker run -ti --volume=${SIT_PSDM_DATA}:/reg/d/psdm --volume=${PSANA_LEGION_DIR}/..:/psana-legion-dev --volume=${LG_RT_DIR}/.. ${DOCKER_IMAGE_ID}

.PHONY:	clean
clean:
	rm -f ${DOCKER_IMAGE_ID_FILE}
