#!/bin/bash

set -e

root_dir="$(dirname "${BASH_SOURCE[0]}")"
cd "$root_dir"

source ../setup/env.sh

mkdir -p build
cd build
cmake ..
make -j8
