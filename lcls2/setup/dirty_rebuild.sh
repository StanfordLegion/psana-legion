#!/bin/bash

set -e

root_dir="$(dirname "${BASH_SOURCE[0]}")"
source "$root_dir"/env.sh

pushd $LCLS2_DIR
./build_all.sh -d # -p install
# if [[ $(hostname --fqdn) != *"summit"* ]]; then
#     pytest psana/psana/tests
# fi
popd
