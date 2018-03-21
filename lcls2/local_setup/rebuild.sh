#!/bin/bash

set -e

source env.sh

pushd $LCLS2_PREFIX
  install_dir=$LCLS2_PREFIX/install
  rm -rf $install_dir
  mkdir $install_dir

  rm -rf xtcdata/build
  mkdir xtcdata/build
  pushd xtcdata/build
    cmake -DCMAKE_INSTALL_PREFIX=$install_dir ..
    make -j8 install
  popd

  rm -rf $install_dir/lib/python$PYVER/site-packages
  mkdir -p $install_dir/lib/python$PYVER/site-packages

  pushd psana
    python setup.py install --xtcdata=$install_dir --prefix=$install_dir
  popd
popd
