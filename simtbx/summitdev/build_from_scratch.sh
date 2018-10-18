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

# Setup environment.
cat > env.sh <<EOF
module load gcc/6.3.1-20170301
module load cuda/9.0.69

# variables needed for conda
export CONDA_PREFIX=$PWD/conda

export PATH=\$CONDA_PREFIX/bin:\$PATH
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
rm -rf conda
rm -rf cctbx

source env.sh

# Install Conda environment.
wget https://repo.continuum.io/miniconda/Miniconda2-latest-Linux-ppc64le.sh
bash Miniconda2-latest-Linux-ppc64le.sh -b -p $CONDA_PREFIX
rm Miniconda2-latest-Linux-ppc64le.sh
source $CONDA_PREFIX/etc/profile.d/conda.sh

conda create -y --name myenv --file dials_env.txt --channel cctbx --channel conda-forge --channel defaults --channel bioconda --override-channels
conda activate myenv
python -m pip install orderedset
python -m pip install procrunner

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
    python $CCTBX_PREFIX/modules/cctbx_project/libtbx/configure.py --enable_openmp_if_possible=True --enable_cuda LS49 prime iota
    source $CCTBX_PREFIX/build/setpaths.sh
    make
  popd
popd

echo
echo "Done. Please run 'source env.sh' to use this build."
