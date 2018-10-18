#!/bin/bash

set -e

# Setup environment.
cat > env.sh <<EOF
export CONDA_PREFIX=$PWD/conda
export REL_DIR=\$CONDA_PREFIX/myrel

export PYVER=3.6

export PATH=\$CONDA_PREFIX/bin:\$PATH
export LD_LIBRARY_PATH=\$REL_DIR/lib:\$LD_LIBRARY_PATH

if [[ -d \$REL_DIR ]]; then
  source activate \$REL_DIR
fi
EOF

# Clean up any previous installs.
rm -rf conda
rm -rf relmanage

source env.sh

# Get recipes.
git clone https://github.com/slac-lcls/relmanage.git

# Install Conda environment.
wget https://repo.continuum.io/miniconda/Miniconda3-latest-Linux-x86_64.sh
bash Miniconda3-latest-Linux-x86_64.sh -b -p $CONDA_PREFIX
rm Miniconda3-latest-Linux-x86_64.sh
conda update -y conda
sed s/PYTHONVER/$PYVER/ relmanage/env_create.yaml > temp_env_create.yaml
conda env create -p $REL_DIR -f temp_env_create.yaml
source activate $REL_DIR

conda remove -y legion

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
    -DCMAKE_INSTALL_PREFIX="$REL_DIR" \
    -DCMAKE_INSTALL_LIBDIR="$REL_DIR/lib" \
    ..
make -j8
make install
popd

echo
echo "Done. Please run 'source env.sh' to use this build."
