#!/bin/bash
echo run this script with sudo from the psana-legion/docker directory
echo after this run shifter_pull_alan.bash on cori to pull the docker image
docker login && docker build -t stanfordlegion/psana-legion:alan -f Dockerfile.psana-legion . && docker push stanfordlegion/psana-legion:alan
