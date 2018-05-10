#!/bin/bash
#docker login && docker build -t stanfordlegion/psana-legion -f Dockerfile.psana-legion . && docker push stanfordlegion/psana-legion
echo run this script with sudo from the psana-legion/docker directory
docker login && docker build -t stanfordlegion/psana-legion:alan -f Dockerfile.psana-legion . && docker push stanfordlegion/psana-legion:alan
