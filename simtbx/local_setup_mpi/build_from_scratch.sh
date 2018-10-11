#!/bin/bash

set -e

if [[ -z $LS49_BIG_DATA ]]; then
    echo "Please set LS49_BIG_DATA and run again"
    exit 1
fi
if [[ ! -f $LS49_BIG_DATA/1m2a.pdb ]]; then
    echo "LS49_BIG_DATA does not contain a file named '1m2a.pdb', are you sure it's pointing to the right place?"
    exit 1
fi

if [[ -z $USE_BYFL ]]; then
    echo "Please set USE_BYFL to 0 or 1 and run again"
    exit 1
fi
if [[ $USE_BYFL -eq 1 && -z $BYFL_PREFIX ]]; then
    echo "Please set BYFL_PREFIX and run again"
    exit 1
fi

# Setup environment.
cat > env.sh <<EOF
# variables needed for Byfl
export USE_BYFL=$USE_BYFL
export BYFL_PREFIX=$BYFL_PREFIX
export BYFL_WRAPPER_DIR=$PWD/byfl_wrapper
export BF_CLANG=clang-6.0
export BF_CLANGXX=clang++-6.0
export BF_OPTS="-bf-disable=byfl"

# variables needed for conda
export CONDA_PREFIX=$PWD/conda

export PATH=\$CONDA_PREFIX/bin:\$BYFL_WRAPPER_DIR:\$PATH
export LD_LIBRARY_PATH=\$CONDA_PREFIX/lib:\$LD_LIBRARY_PATH

# variables needed for CCTBX
export CCTBX_PREFIX=$PWD/cctbx

# variables needed for run only
export LS49_BIG_DATA=$LS49_BIG_DATA

# variables needed to run CCTBX
if [[ -d \$CONDA_PREFIX ]]; then
  source \$CONDA_PREFIX/etc/profile.d/conda.sh
  conda activate myenv
fi
if [[ -e \$CCTBX_PREFIX/build/setpaths.sh ]]; then
  source \$CCTBX_PREFIX/build/setpaths.sh
fi
EOF

root_dir=$PWD

# Clean up any previous installs.
rm -rf byfl_wrapper
rm -rf conda
rm -rf cctbx

source env.sh

# Install Conda environment.
wget https://repo.continuum.io/miniconda/Miniconda2-latest-Linux-x86_64.sh
bash Miniconda2-latest-Linux-x86_64.sh -b -p $CONDA_PREFIX
rm Miniconda2-latest-Linux-x86_64.sh
source $CONDA_PREFIX/etc/profile.d/conda.sh

curl -O https://raw.githubusercontent.com/nksauter/LS49/master/tests/dials_env.txt
conda create -y --name myenv --file dials_env.txt --channel cctbx --channel conda-forge --channel defaults --channel bioconda --override-channels
rm dials_env.txt
conda activate myenv
python -m pip install procrunner

if [[ $USE_BYFL -eq 1 ]]; then
  mkdir $BYFL_WRAPPER_DIR
  cat >$BYFL_WRAPPER_DIR/gcc <<EOF
$BYFL_PREFIX/bin/bf-clang "\$@"
EOF
  cat >$BYFL_WRAPPER_DIR/g++ <<EOF
$BYFL_PREFIX/bin/bf-clang++ "\$@"
EOF
  chmod +x $BYFL_WRAPPER_DIR/gcc $BYFL_WRAPPER_DIR/g++

  # ln -s $BYFL_PREFIX/bin/bf-clang $BYFL_WRAPPER_DIR/gcc
  # ln -s $BYFL_PREFIX/bin/bf-clang++ $BYFL_WRAPPER_DIR/g++
fi

# Build CCTBX.
mkdir $CCTBX_PREFIX
pushd $CCTBX_PREFIX
  curl -O https://raw.githubusercontent.com/cctbx/cctbx_project/master/libtbx/auto_build/bootstrap.py
  python bootstrap.py hot --builder=dials
  python bootstrap.py update --builder=dials
  pushd $CCTBX_PREFIX/modules
    git clone https://github.com/nksauter/LS49.git
  popd
  pushd $CCTBX_PREFIX/modules/cctbx_project
    git remote add elliott https://github.com/elliottslaughter/cctbx_project.git
    git fetch elliott
    git checkout simtbx-cuda-workaround
  popd
  cp $LS49_BIG_DATA/nanoBraggCUDA.cu $CCTBX_PREFIX/modules/cctbx_project/simtbx/nanoBragg
  mkdir $CCTBX_PREFIX/build
  pushd $CCTBX_PREFIX/build
    # Build first time with Byfl disabled
    export BF_OPTS="-bf-disable=byfl"
    if [[ $USE_BYFL -eq 1 ]]; then
      python $CCTBX_PREFIX/modules/cctbx_project/libtbx/configure.py --enable_openmp_if_possible=False LS49 prime iota
    else
      python $CCTBX_PREFIX/modules/cctbx_project/libtbx/configure.py --enable_openmp_if_possible=True --enable_cuda LS49 prime iota
    fi
    source $CCTBX_PREFIX/build/setpaths.sh
    make

    # Now rebuild with Byfl enabled.
    if [[ $USE_BYFL -eq 1 ]]; then
      (
        export BF_OPTS="-bf-by-func -bf-call-stack"
        rm simtbx/nanoBragg/*.o
        make
      )
    fi
  popd
popd

echo
echo "Done. Please run 'source env.sh' to use this build."
