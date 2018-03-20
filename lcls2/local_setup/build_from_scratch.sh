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
export PATH=\$REL_PREFIX/bin:\$CONDA_PREFIX/bin:\$PATH

export PYVER=3.5
export LCLS2_PREFIX=$PWD/lcls2
export PYTHONPATH=\$LCLS2_PREFIX/install/lib/python\$PYVER/site-packages:\$PYTHONPATH
EOF

source env.sh

# Clean up any previous installs.
rm -rf $CONDA_PREFIX
rm -rf channels
rm -rf relmanage
rm -rf $LCLS2_PREFIX

# Install Conda environment.
wget https://repo.continuum.io/miniconda/Miniconda3-latest-Linux-x86_64.sh
bash Miniconda3-latest-Linux-x86_64.sh -b -p $CONDA_PREFIX
rm Miniconda3-latest-Linux-x86_64.sh
conda update -y conda
conda install -y conda-build # Must be installed in root environment
conda create -y -p $REL_PREFIX python=$PYVER cmake h5py ipython numpy cython
source activate $REL_PREFIX
# conda install -y --channel lcls-rhel7 cpsw yaml-cpp
# conda install -y --channel lightsource2-tag epics-base

# Install Legion.
git clone https://github.com/slac-lcls/relmanage.git
conda build relmanage/recipes/legion/ --output-folder channels/external/
conda install -y legion -c file://`pwd`/channels/external # --override-channels

# Build psana.
git clone https://github.com/slac-lcls/lcls2.git $LCLS2_PREFIX
pushd $LCLS2_PREFIX
  install_dir=$LCLS2_PREFIX/install
  mkdir $install_dir

  mkdir xtcdata/build
  pushd xtcdata/build
    cmake -DCMAKE_INSTALL_PREFIX=$install_dir ..
    make -j8 install
  popd

  mkdir -p $install_dir/lib/python$PYVER/site-packages

  pushd psana
    python setup.py install --xtcdata=$install_dir --prefix=$install_dir
  popd
popd

echo
echo "Done. Please run 'source env.sh' to use this build."
