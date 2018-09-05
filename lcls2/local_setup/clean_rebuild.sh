#!/bin/bash

set -e

source env.sh
source activate $REL_PREFIX

pushd $LCLS2_PREFIX
git clean -fxd
popd

./dirty_rebuild.sh
