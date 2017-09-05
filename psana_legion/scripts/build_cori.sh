#!/bin/bash

FORCE_PYTHON=1 PYTHON_LIB=/conda/lib/libpython2.7.so LG_RT_DIR=$HOME/psana_legion/legion/runtime DEBUG=0 make -j16
