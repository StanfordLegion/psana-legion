#!/bin/bash

set -e

source env.sh
source activate $REL_PREFIX

pushd $LCLS2_PREFIX
./build_all.sh -d -p install
pytest psana/psana/tests
popd
