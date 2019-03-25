#!/bin/bash

set -e

# Setup environment.
if [[ $(hostname --fqdn) = *"summit"* ]]; then
    cat > env.sh <<EOF
module load gcc/6.4.0
module load cuda/9.2.148
export CC=gcc
export CXX=g++

export USE_CUDA=${USE_CUDA:-1}
export USE_GASNET=${USE_GASNET:-1}
export CONDUIT=${CONDUIT:-ibv}
EOF
else
    if [[ -z $CC || -z $CXX || ! $($CC --version) = *" 6."* || ! $($CXX --version) = *" 6."* ]]; then
        echo "GCC 6.x is required to build."
        echo "Please set CC/CXX to the right version and run again."
        echo
        echo "Note: This means machine auto-detection failed."
        exit 1
    fi
    cat > env.sh <<EOF
export CC="$CC"
export CXX="$CXX"

export USE_CUDA=${USE_CUDA:-0}
export USE_GASNET=${USE_GASNET:-0}
export CONDUIT=${CONDUIT:-mpi}
EOF
fi

cat >> env.sh <<EOF
export GASNET_ROOT="${GASNET_ROOT:-$PWD/gasnet/release}"

export LG_RT_DIR="${LG_RT_DIR:-$PWD/legion/runtime}"

export CONDA_ROOT="$PWD/conda"

export PYVER=3.6
export LCLS2_DIR="$PWD/lcls2"

export PATH="\$LCLS2_DIR/install/bin:\$PATH"
export PYTHONPATH="\$LCLS2_DIR/install/lib/python\$PYVER/site-packages:\$PYTHONPATH"

if [[ -d \$CONDA_ROOT ]]; then
  source "\$CONDA_ROOT/etc/profile.d/conda.sh"
  conda activate myenv
  export LD_LIBRARY_PATH="\$CONDA_PREFIX/lib:\$LD_LIBRARY_PATH"
fi
EOF

# Clean up any previous installs.
rm -rf conda
rm -rf channels
rm -rf relmanage
rm -rf lcls2

source env.sh

# Install Conda environment.
wget https://repo.continuum.io/miniconda/Miniconda3-latest-Linux-$(uname -p).sh -O conda-installer.sh
bash ./conda-installer.sh -b -p $CONDA_ROOT
rm conda-installer.sh
source $CONDA_ROOT/etc/profile.d/conda.sh

# conda install -y conda-build # Must be installed in root environment
PACKAGE_LIST=(
    # Stripped down from env_create.yaml:
    python=$PYVER
    cmake
    numpy
    cython
    matplotlib
    pytest
    mongodb
    pymongo
    curl
    rapidjson
    ipython
    requests

    # Legion dependencies:
    cffi
)
if [[ $(hostname --fqdn) != *"summit"* ]]; then
    PACKAGE_LIST+=(
        mpi4py
    )
fi
conda create -y --name myenv "${PACKAGE_LIST[@]}"
# FIXME: Can't do this on Summit since not all the packages are available....
# git clone https://github.com/slac-lcls/relmanage.git
# sed s/PYTHONVER/$PYVER/ relmanage/env_create.yaml > temp_env_create.yaml
# conda env create --name myenv -f temp_env_create.yaml
conda activate myenv
export LD_LIBRARY_PATH="$CONDA_PREFIX/lib:$LD_LIBRARY_PATH"

# Workaround for mpi4py not being built with the right MPI.
if [[ $(hostname --fqdn) = *"summit"* ]]; then
    CC=$OMPI_CC MPICC=mpicc pip install -v --no-binary mpi4py mpi4py
fi

# Install Legion.
# conda build relmanage/recipes/legion/ --output-folder channels/external/ --python $PYVER
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
    git clone -b cmake-gasnet-private-dependency-dcr git@gitlab.com:StanfordLegion/legion.git
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
        -DCMAKE_INSTALL_PREFIX="$CONDA_PREFIX" \
        -DCMAKE_INSTALL_LIBDIR="$CONDA_PREFIX/lib" \
        ..
    make -j8
    make install
    popd
fi

# Build psana.
git clone https://github.com/slac-lcls/lcls2.git $LCLS2_DIR
./clean_rebuild.sh

echo
echo "Done. Please run 'source env.sh' to use this build."
