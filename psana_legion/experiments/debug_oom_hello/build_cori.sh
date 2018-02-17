#!/bin/bash

export GASNET=$HOME/psana_legion/gasnet/release
export CONDUIT=aries
export USE_GASNET=1
export USE_LIBDL=0

export LG_RT_DIR=$HOME/psana_legion/legion/runtime

make clean
DEBUG=0 make -j16
