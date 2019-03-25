#!/bin/bash

set -e

source env.sh

pushd $LCLS2_DIR
git clean -fxd
popd

./dirty_rebuild.sh
