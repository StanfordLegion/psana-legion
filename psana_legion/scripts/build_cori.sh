#!/bin/bash

export GASNET=$HOME/psana_legion/gasnet/release
export CONDUIT=aries
export USE_GASNET=1

export LG_RT_DIR=$HOME/psana_legion/legion/runtime

make clean
FORCE_PYTHON=1 PYTHON_LIB=/conda/lib/libpython2.7.so DEBUG=0 make -j16
