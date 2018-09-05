#!/bin/bash

set -e

source env.sh
source activate $REL_PREFIX

pushd $LCLS2_PREFIX
./build_python3_light.sh
pytest psana/psana/tests
popd
