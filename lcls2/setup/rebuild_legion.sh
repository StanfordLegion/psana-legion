#!/bin/bash

set -e

root_dir="$(dirname "${BASH_SOURCE[0]}")"
source "$root_dir"/env.sh

pushd "$root_dir"/legion/build

make -j8
make install

popd
