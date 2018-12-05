#!/bin/bash

set -e

if [[ -z $CC || -z $CXX || ! $($CC --version) = *" 6."* || ! $($CXX --version) = *" 6."* ]]; then
    echo "GCC 6.x is required to build"
    echo "Please set CC/CXX to the right version and run again"
    exit 1
fi

# Setup environment.
cat > env.sh <<EOF
export CC=$CC
export CXX=$CXX

export USE_CUDA=${USE_CUDA:-0}
export USE_GASNET=${USE_GASNET:-0}
export CONDUIT=${CONDUIT:-mpi}
export GASNET_ROOT=${GASNET_ROOT:-$PWD/gasnet/release}

export LG_RT_DIR=${LG_RT_DIR:-$PWD/legion/runtime}

export CONDA_PREFIX=$PWD/conda
export REL_DIR=\$CONDA_PREFIX/myrel

export PYVER=3.6
export LCLS2_DIR=$PWD/lcls2

export PATH=\$LCLS2_DIR/install/bin:\$CONDA_PREFIX/bin:\$PATH
export LD_LIBRARY_PATH=\$REL_DIR/lib:\$LD_LIBRARY_PATH
export PYTHONPATH=\$LCLS2_DIR/install/lib/python\$PYVER/site-packages:\$PYTHONPATH

if [[ -d \$REL_DIR ]]; then
  source activate \$REL_DIR
fi
EOF

# Clean up any previous installs.
rm -rf conda
rm -rf channels
rm -rf relmanage
rm -rf lcls2

source env.sh

# Get recipes.
git clone https://github.com/slac-lcls/relmanage.git

# Install Conda environment.
wget https://repo.continuum.io/miniconda/Miniconda3-latest-Linux-x86_64.sh
bash Miniconda3-latest-Linux-x86_64.sh -b -p $CONDA_PREFIX
rm Miniconda3-latest-Linux-x86_64.sh
conda update -y conda
conda install -y conda-build # Must be installed in root environment
# conda create -y -p $REL_DIR python=$PYVER cmake h5py ipython numpy cython nose
sed s/PYTHONVER/$PYVER/ relmanage/env_create.yaml > temp_env_create.yaml
conda env create -p $REL_DIR -f temp_env_create.yaml
source activate $REL_DIR

# # Install Legion.
# conda build relmanage/recipes/legion/ --output-folder channels/external/ --python $PYVER
conda remove -y legion
# conda install -y legion -c file://`pwd`/channels/external --override-channels

if [[ $GASNET_ROOT == $PWD/gasnet/release ]]; then
    rm -rf gasnet
    git clone https://github.com/StanfordLegion/gasnet.git
    pushd gasnet
    make -j8
    popd
fi

if [[ $LG_RT_DIR == $PWD/legion/runtime ]]; then
    rm -rf legion
    git clone -b cmake-gasnet-private-dependency git@gitlab.com:StanfordLegion/legion.git
    pushd legion
    mkdir build
    cd build
    cmake -DBUILD_SHARED_LIBS=ON \
        -DLegion_BUILD_BINDINGS=ON \
        -DLegion_ENABLE_TLS=ON \
        -DLegion_USE_Python=ON \
        -DPYTHON_EXECUTABLE="$(which python)" \
        -DLegion_USE_CUDA=$([ $USE_CUDA -eq 1 ] && echo ON || echo OFF) \
        -DLegion_USE_GASNet=$([ $USE_GASNET -eq 1 ] && echo ON || echo OFF) \
        -DGASNet_ROOT_DIR="$GASNET_ROOT" \
        -DGASNet_CONDUITS=$CONDUIT \
        -DCMAKE_INSTALL_PREFIX="$REL_DIR" \
        -DCMAKE_INSTALL_LIBDIR="$REL_DIR/lib" \
        ..
    make -j8
    make install
    popd
fi

# Build psana.
# git clone https://github.com/slac-lcls/lcls2.git $LCLS2_DIR
git clone -b more_events https://github.com/elliottslaughter/lcls2.git $LCLS2_DIR
./clean_rebuild.sh

echo
echo "Done. Please run 'source env.sh' to use this build."
