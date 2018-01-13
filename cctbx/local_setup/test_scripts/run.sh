#!/bin/bash

rm -rf output
mkdir output

mpirun -n 4 ./index.sh cxid9114 108 0
