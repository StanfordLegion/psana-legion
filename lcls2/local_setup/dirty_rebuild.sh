#!/bin/bash

set -e

source env.sh

pushd $LCLS2_DIR
./build_all.sh -d -p install
pytest psana/psana/tests
popd
