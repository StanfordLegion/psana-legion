#!/bin/bash

set -e

# Setup environment.
cat > env.sh <<EOF
module load gcc/7.1.0
export CC=gcc
export CXX=g++

export CONDA_PREFIX=$PWD/conda
export REL_DIR=\$CONDA_PREFIX/myrel
export PATH=\$REL_DIR/bin:\$CONDA_PREFIX/bin:\$PATH
EOF

source env.sh

# Clean up any previous installs.

rm -rf $CONDA_PREFIX
rm -rf channels
rm -rf relmanage

# Install Conda environment.
wget https://repo.continuum.io/miniconda/Miniconda3-latest-Linux-ppc64le.sh
bash Miniconda3-latest-Linux-ppc64le.sh -b -p $CONDA_PREFIX
rm Miniconda3-latest-Linux-ppc64le.sh
conda update -y conda
conda install -y conda-build # Must be installed in root environment
conda create -y -p $REL_DIR python=3.5 cmake h5py ipython numpy
source activate $REL_DIR
# conda install -y --channel lcls-rhel7 cpsw yaml-cpp
# conda install -y --channel lightsource2-tag epics-base

# Install Legion
git clone https://github.com/slac-lcls/relmanage.git
conda build relmanage/recipes/legion/ --output-folder channels/external/
conda install -y legion -c file://`pwd`/channels/external --override-channels
