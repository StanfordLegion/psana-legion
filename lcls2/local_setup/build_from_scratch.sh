#!/bin/bash

set -e

if [[ -z $CC || -z $CXX || ! $($CC --version) = *" 7."* || ! $($CXX --version) = *" 7."* ]]; then
    echo "GCC 7 is required to build"
    echo "Please set CC/CXX to the right version and run again"
    exit 1
fi

# Setup environment.
cat > env.sh <<EOF
export CC=$CC
export CXX=$CXX

export CONDA_PREFIX=$PWD/conda
export REL_PREFIX=\$CONDA_PREFIX/myrel

export PYVER=3.6
export LCLS2_PREFIX=$PWD/lcls2

export PATH=\$LCLS2_PREFIX/install/bin:\$REL_PREFIX/bin:\$CONDA_PREFIX/bin:\$PATH
export PYTHONPATH=\$LCLS2_PREFIX/install/lib/python\$PYVER/site-packages:\$PYTHONPATH
EOF

source env.sh

# Clean up any previous installs.
rm -rf $CONDA_PREFIX
rm -rf channels
rm -rf relmanage
rm -rf $LCLS2_PREFIX

# Get recipes.
git clone https://github.com/slac-lcls/relmanage.git
sed -i 's@- file:///reg/g/psdm/sw/conda2/channels/external@@g' relmanage/env_create.yaml
sed -i 's@- file:///reg/g/pcds/pyps/conda/channel@@g' relmanage/env_create.yaml
sed -i 's@- legion@@g' relmanage/env_create.yaml
sed -i 's@- cpsw@@g' relmanage/env_create.yaml
sed -i 's@- procserv@@g' relmanage/env_create.yaml
sed -i 's@- pyca@@g' relmanage/env_create.yaml
sed -i 's@- libfabric@@g' relmanage/env_create.yaml
sed -i 's@- psmon@@g' relmanage/env_create.yaml
mkdir -p channels/external

# Install Conda environment.
wget https://repo.continuum.io/miniconda/Miniconda3-latest-Linux-x86_64.sh
bash Miniconda3-latest-Linux-x86_64.sh -b -p $CONDA_PREFIX
rm Miniconda3-latest-Linux-x86_64.sh
conda update -y conda
conda install -y conda-build # Must be installed in root environment
# conda create -y -p $REL_PREFIX python=$PYVER cmake h5py ipython numpy cython nose
conda env create -p $REL_PREFIX -f relmanage/env_create.yaml
source activate $REL_PREFIX
conda install libcurl
# conda install -y --channel lcls-rhel7 cpsw yaml-cpp
# conda install -y --channel lightsource2-tag epics-base

# Install Legion.
conda build relmanage/recipes/legion/ --output-folder channels/external/
conda install -y legion -c file://`pwd`/channels/external # --override-channels

# Build psana.
git clone https://github.com/slac-lcls/lcls2.git $LCLS2_PREFIX
./clean_rebuild.sh

echo
echo "Done. Please run 'source env.sh' to use this build."
