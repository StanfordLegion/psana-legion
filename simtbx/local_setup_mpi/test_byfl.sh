#!/bin/bash

set -e

source env.sh

rm -rf test_byfl
mkdir test_byfl
pushd test_byfl
  export TEST_NUM_MOSAIC_DOMAINS=1
  time libtbx.python $CCTBX_PREFIX/modules/LS49/tests/tst_monochromatic_image.py
popd
