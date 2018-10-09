#!/bin/bash

set -e

# Setup environment
cat > env.sh <<EOF
module load gcc/6.3.1-20170301
module load cuda/9.0.69
export CC=gcc
export CXX=g++

export USE_GASNET=0
export USE_CUDA=1
export CONDUIT=ibv
export GASNET_NUM_QPS=1 # FIXME: https://upc-bugs.lbl.gov/bugzilla/show_bug.cgi?id=3447

export CONDA_PREFIX=$PWD/conda
export REL_DIR=\$CONDA_PREFIX/myrel
export PATH=\$CONDA_PREFIX/bin:\$PATH

if [[ -d \$REL_DIR ]]; then
  source activate \$REL_DIR
fi

export LCLS2_DIR=$PWD/lcls2
export PYTHONPATH=\$LCLS2_DIR/install/lib/python3.6/site-packages:\$PYTHONPATH
EOF

# Clean up any previous installs
rm -rf conda
rm -rf channels
rm -rf relmanage
rm -rf lcls2

source env.sh

# Install Conda environment
wget https://repo.continuum.io/miniconda/Miniconda3-latest-Linux-ppc64le.sh
bash Miniconda3-latest-Linux-ppc64le.sh -b -p $CONDA_PREFIX
rm Miniconda3-latest-Linux-ppc64le.sh
conda update -y conda
conda install -y conda-build # Must be installed in root environment
conda create -y -p $REL_DIR python=3.6 cmake h5py ipython numpy cffi curl cython rapidjson pytest requests pymongo mongodb
source activate $REL_DIR
CC=$OMPI_CC MPICC=mpicc pip install -v --no-binary mpi4py mpi4py

# Install Legion
# git clone https://github.com/slac-lcls/relmanage.git
# conda build relmanage/recipes/legion/ --output-folder channels/external/ --python 3.6
# conda install -y legion -c file://`pwd`/channels/external # --override-channels

rm -rf legion
git clone -b cmake-gasnet-private-dependency git@gitlab.com:StanfordLegion/legion.git
cd legion
mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=ON \
    -DLegion_BUILD_BINDINGS=ON \
    -DLegion_ENABLE_TLS=ON \
    -DLegion_USE_Python=ON \
    -DPYTHON_EXECUTABLE="$(which python)" \
    -DCMAKE_INSTALL_PREFIX="$REL_DIR" \
    -DCMAKE_INSTALL_LIBDIR="$REL_DIR/lib" \
    ..
make -j
make install

# Build
git clone https://github.com/slac-lcls/lcls2.git "$LCLS2_DIR"
pushd "$LCLS2_DIR"
./build_all.sh -d -p install
pytest psana/psana/tests
popd
