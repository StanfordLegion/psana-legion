#!/bin/bash

set -e

root_dir="$(dirname "${BASH_SOURCE[0]}")"
source "$root_dir"/env.sh

rm -rf "$root_dir"/legion/build
mkdir "$root_dir"/legion/build
pushd "$root_dir"/legion/build

cmake -DCMAKE_BUILD_TYPE=$([ $LEGION_DEBUG -eq 1 ] && echo Debug || echo Release) \
    -DBUILD_SHARED_LIBS=ON \
    -DLegion_BUILD_BINDINGS=ON \
    -DLegion_ENABLE_TLS=ON \
    -DLegion_USE_Python=ON \
    -DPYTHON_EXECUTABLE="$(which python)" \
    -DLegion_USE_CUDA=$([ $USE_CUDA -eq 1 ] && echo ON || echo OFF) \
    -DLegion_USE_GASNet=$([ $USE_GASNET -eq 1 ] && echo ON || echo OFF) \
    -DGASNet_ROOT_DIR="$GASNET_ROOT" \
    -DGASNet_CONDUITS=$CONDUIT \
    -DLegion_MAX_DIM=$MAX_DIM \
    -DCMAKE_INSTALL_PREFIX="$LEGION_INSTALL_DIR" \
    -DCMAKE_INSTALL_LIBDIR="$LEGION_INSTALL_DIR/lib" \
    ..

popd
