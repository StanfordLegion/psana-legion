#!/bin/bash

set -e

if [[ -z $LG_RT_DIR ]]; then
    echo "Please set LG_RT_DIR and run again"
    exit 1
fi
if [[ -z $PSANA_LEGION_DIR ]]; then
    echo "Please set PSANA_LEGION_DIR and run again"
    exit 1
fi
if [[ -z $SIT_PSDM_DATA ]]; then
    echo "Please set SIT_PSDM_DATA and run again"
    exit 1
fi
if [[ ! -d $SIT_PSDM_DATA/xpp && ! -d $SIT_PSDM_DATA/cxi ]]; then
    echo "SIT_PSDM_DATA does not contain a subdirectory named 'xpp' (or 'cxi'), are you sure it's pointing to the right place?"
    exit 1
fi

# Setup environment.
cat > env.sh <<EOF
# source directories
export LG_RT_DIR=$LG_RT_DIR
export PSANA_LEGION_DIR=$PSANA_LEGION_DIR

# variables needed for GASNet
export GASNET_ROOT_DIR=$PWD/gasnet-mpi
export GASNET=\$GASNET_ROOT_DIR/release
export CONDUIT=mpi

# variables needed for conda
export SIT_ARCH=x86_64-rhel7-gcc48-opt
export CONDA_PREFIX=$PWD/conda
export REL_PREFIX=\$CONDA_PREFIX/myrel

export PATH=\$REL_PREFIX/arch/\$SIT_ARCH/bin:\$CONDA_PREFIX/bin:\$PATH
export LD_LIBRARY_PATH=\$REL_PREFIX/arch/\$SIT_ARCH/lib:\$CONDA_PREFIX/lib:\$PSANA_LEGION_DIR:\$LD_LIBRARY_PATH
export PYTHONPATH=.:\$REL_PREFIX/arch/\$SIT_ARCH/python:\$PSANA_LEGION_DIR:\$PYTHONPATH

# variables needed for scons only
if which conda; then
  export SIT_RELEASE=\$(conda list | grep psana-conda | tr -s ' ' | cut -f1-2 -d' ' | tr ' ' '-')
fi
export SIT_REPOS="\$CONDA_PREFIX/data/anarelinfo"
export SIT_USE_CONDA=1

# variables needed for legion
export USE_PYTHON=1
export USE_GASNET=1
export DEBUG=1
unset WARN_AS_ERROR

# variables needed for run only
export SIT_PSDM_DATA=$SIT_PSDM_DATA
#export EXPERIMENT="exp=xpptut15:run=54:rax"
#export DETECTOR=cspad
EOF

source env.sh

# Clean up any previous installs.
rm -rf $CONDA_PREFIX
rm -rf $GASNET_ROOT_DIR

# Install Conda environment.
wget https://repo.continuum.io/miniconda/Miniconda2-latest-Linux-x86_64.sh
bash Miniconda2-latest-Linux-x86_64.sh -b -p $CONDA_PREFIX
rm Miniconda2-latest-Linux-x86_64.sh
conda install -y scons cython libtiff=4.0.6 icu=54
conda install -y --channel lcls-rhel7 psana-conda ndarray

# Build GASNet.
git clone https://github.com/StanfordLegion/gasnet.git $GASNET_ROOT_DIR
pushd $GASNET_ROOT_DIR
make
popd

# Build Legion.
pushd "$PSANA_LEGION_DIR"
  make clean
  make -j 8
popd

# Build psana.
mkdir "$REL_PREFIX"
pushd "$REL_PREFIX"
  ln -s "$CONDA_PREFIX/lib/python2.7/site-packages/SConsTools/SConstruct.main" SConstruct
  export SIT_RELEASE=$(conda list | grep psana-conda | tr -s ' ' | cut -f1-2 -d' ' | tr ' ' '-')
  echo "$SIT_RELEASE" > .sit_release
  echo "$CONDA_PREFIX" > .sit_conda_env
  git clone -b legion https://github.com/elliottslaughter/psana.git
  git clone -b legion https://github.com/elliottslaughter/psana_python.git
  git clone https://github.com/lcls-psana/python.git
  git clone https://github.com/lcls-psana/numpy.git
  git clone -b legion https://github.com/elliottslaughter/PSXtcInput.git
  git clone https://github.com/lcls-psana/SConsTools.git
  scons
popd

echo
echo "Done. Please run 'source env.sh' to use this build."
