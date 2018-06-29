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
if [[ ! -d $SIT_PSDM_DATA/cxi ]]; then
    echo "SIT_PSDM_DATA does not contain a subdirectory named 'cxi', are you sure it's pointing to the right place?"
    exit 1
fi
if [[ ! -e phenix-installer.tar.gz ]]; then
    echo "Please download Phenix from http://phenix-online.org"
    echo "And name the file phenix-installer.tar.gz"
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

export GCC_WRAPPER_DIR=$PWD/gcc_wrapper

# FIXME: Scons doesn't like having REL_PREFIX on PATH???
# export PATH=\$REL_PREFIX/arch/\$SIT_ARCH/bin:\$CONDA_PREFIX/bin:\$GCC_WRAPPER_DIR:\$PATH
export PATH=\$CONDA_PREFIX/bin:\$GCC_WRAPPER_DIR:\$PATH
export LD_LIBRARY_PATH=\$REL_PREFIX/arch/\$SIT_ARCH/lib:\$CONDA_PREFIX/lib:\$LD_LIBRARY_PATH
export PYTHONPATH=\$REL_PREFIX/arch/\$SIT_ARCH/python:\$PYTHONPATH

# variables needed for CCTBX
export CCTBX_PREFIX=$PWD/cctbx

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

# variables needed to run CCTBX
if [[ -e \$CCTBX_PREFIX/build/setpaths.sh ]]; then
  source \$CCTBX_PREFIX/build/setpaths.sh
fi
EOF

root_dir=$PWD

source env.sh

# Clean up any previous installs.
rm -rf $CONDA_PREFIX
rm -rf $CCTBX_PREFIX
rm -rf $GASNET_ROOT_DIR
rm -rf $GCC_WRAPPER_DIR

# Install Conda environment.
wget https://repo.continuum.io/miniconda/Miniconda2-latest-Linux-x86_64.sh
bash Miniconda2-latest-Linux-x86_64.sh -b -p $CONDA_PREFIX
rm Miniconda2-latest-Linux-x86_64.sh
conda install -y scons cython libtiff=4.0.6 icu=54 future wxpython pillow mock pytest jinja2 scikit-learn tabulate
conda install -y --channel conda-forge "mpich>=3" mpi4py h5py pytables orderedset
# The --no-deps flag doesn't look at dependencies *at all*, so to
# ensure the right dependencies are in place, install psana first,
# then upgrade numpy (removes psana), then install psana --no-deps.
conda install -y --channel lcls-rhel7 psana-conda ndarray
conda install -y numpy=1.13.3
conda install -y --no-deps --channel lcls-rhel7 psana-conda
python -m pip install procrunner

# Build GASNet.
git clone https://github.com/StanfordLegion/gasnet.git $GASNET_ROOT_DIR
pushd $GASNET_ROOT_DIR
make
popd

# Build Legion.
pushd "$PSANA_LEGION_DIR"
  make clean
  make -j $(nproc --all)
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

# CCTBX can't handle the new ABIs in GCC >= 5. Since the CCTBX build
# system doesn't recognize the CXX environment variable, we use a
# wrapper to pass the necessary flags.
mkdir $GCC_WRAPPER_DIR
cat > $GCC_WRAPPER_DIR/g++ <<EOF
#!/bin/bash

$(which g++) -std=c++11 -fabi-version=2 -D_GLIBCXX_USE_CXX11_ABI=0 "\$@"
EOF
chmod +x $GCC_WRAPPER_DIR/g++

# Build CCTBX.
mkdir $CCTBX_PREFIX
pushd $CCTBX_PREFIX
  tar xfzv $root_dir/phenix-installer.tar.gz --strip 1 --wildcards '*/modules/labelit'
  curl -O https://raw.githubusercontent.com/cctbx/cctbx_project/master/libtbx/auto_build/bootstrap.py
  mkdir -p modules/cxi_xdr_xes
  python bootstrap.py hot --builder=xfel
  python bootstrap.py update --builder=dials
  pushd modules/cctbx_project
    git remote add elliott https://github.com/elliottslaughter/cctbx_project.git
    git fetch elliott
    git checkout -b psana-tasking elliott/psana-tasking
  popd
  python bootstrap.py build --builder=xfel --with-python=$CONDA_PREFIX/bin/python --nproc $(nproc --all)
popd

echo
echo "Done. Please run 'source env.sh' to use this build."
