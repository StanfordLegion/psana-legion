#!/bin/bash
#docker login && docker build -t stanfordlegion/psana-legion -f Dockerfile.psana-legion . && docker push stanfordlegion/psana-legion
docker login && docker build -t stanfordlegion/psana-legion:alan -f Dockerfile.psana-legion . && docker push stanfordlegion/psana-legion:alan
