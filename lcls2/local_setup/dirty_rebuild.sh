#!/bin/bash

set -e

source env.sh

pushd $LCLS2_PREFIX
  install_dir=$LCLS2_PREFIX/install
  mkdir -p $install_dir

  mkdir -p xtcdata/build
  pushd xtcdata/build
    cmake -DCMAKE_INSTALL_PREFIX=$install_dir ..
    make -j8 install
  popd

  mkdir -p $install_dir/lib/python$PYVER/site-packages

  pushd psana
    python setup.py install --xtcdata=$install_dir --legion=$REL_PREFIX --prefix=$install_dir
  popd
popd
