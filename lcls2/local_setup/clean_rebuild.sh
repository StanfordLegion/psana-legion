#!/bin/bash

set -e

source env.sh

install_dir=$LCLS2_PREFIX/install
rm -rf $install_dir
rm -rf $LCLS2_PREFIX/xtcdata/build
rm -rf $LCLS2_PREFIX/psalg/build
./dirty_rebuild.sh
