#!/bin/bash

set -e

source env.sh

pushd $LCLS2_DIR
./build_all.sh -d -p install
if [[ $(hostname --fqdn) != *"summit"* ]]; then
    pytest psana/psana/tests
fi
popd
