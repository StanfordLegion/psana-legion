#!/bin/bash

set -e

source env.sh

rm -rf test_byfl
mkdir test_byfl
pushd test_byfl
  for n in 1 2 5 10 25 50; do
    mkdir test$n
    pushd test$n
      export TEST_NUM_MOSAIC_DOMAINS=$n
      export BF_BINOUT="$PWD"/test.byfl
      time ( libtbx.python $CCTBX_PREFIX/modules/LS49/tests/tst_monochromatic_image.py 2>&1 | tee out.log )
    popd
  done
popd
