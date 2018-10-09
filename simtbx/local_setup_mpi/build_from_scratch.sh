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
# if [[ ! -e phenix-installer.tar.gz ]]; then
#     echo "Please download Phenix from http://phenix-online.org"
#     echo "And name the file phenix-installer.tar.gz"
#     exit 1
# fi

# Setup environment.
cat > env.sh <<EOF
# variables needed for conda
export CONDA_PREFIX=$PWD/conda

export GCC_WRAPPER_DIR=$PWD/gcc_wrapper

export PATH=\$CONDA_PREFIX/bin:\$GCC_WRAPPER_DIR:\$PATH
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
rm -rf gcc_wrapper

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

# # CCTBX can't handle the new ABIs in GCC >= 5. Since the CCTBX build
# # system doesn't recognize the CXX environment variable, we use a
# # wrapper to pass the necessary flags.
# mkdir $GCC_WRAPPER_DIR
# cat > $GCC_WRAPPER_DIR/g++ <<EOF
# #!/bin/bash

# $(which g++) -std=c++11 -fabi-version=2 -D_GLIBCXX_USE_CXX11_ABI=0 "\$@"
# EOF
# chmod +x $GCC_WRAPPER_DIR/g++

# Build CCTBX.
mkdir $CCTBX_PREFIX
pushd $CCTBX_PREFIX
  curl -O https://raw.githubusercontent.com/cctbx/cctbx_project/master/libtbx/auto_build/bootstrap.py
  python bootstrap.py hot --builder=dials
  python bootstrap.py update --builder=dials
  pushd $CCTBX_PREFIX/modules
    git clone https://github.com/nksauter/LS49.git
    cp $LS49_BIG_DATA/nanoBraggCUDA.cu $CCTBX_PREFIX/modules/cctbx_project/simtbx/nanoBragg
  popd
  mkdir $CCTBX_PREFIX/build
  pushd $CCTBX_PREFIX/build
    python $CCTBX_PREFIX/modules/cctbx_project/libtbx/configure.py --enable_openmp_if_possible=True --enable_cuda LS49 prime iota
    source $CCTBX_PREFIX/build/setpaths.sh
    make
  popd
popd

echo
echo "Done. Please run 'source env.sh' to use this build."
