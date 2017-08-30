FROM centos:centos7
MAINTAINER Elliott Slaughter <slaughter@cs.stanford.edu>

RUN yum clean all && \
    yum -y install bzip2 gcc-c++ git shasum

ENV CONDA_PREFIX /conda
ENV REL_PREFIX /conda/myrel
ENV SIT_ARCH x86_64-rhel7-gcc48-opt
ENV PATH $REL_PREFIX/arch/$SIT_ARCH/bin:$CONDA_PREFIX/bin:$PATH
ENV LD_LIBRARY_PATH $REL_PREFIX/arch/$SIT_ARCH/lib:$CONDA_PREFIX/lib:$LD_LIBRARY_PATH
ENV PYTHONPATH $REL_PREFIX/arch/$SIT_ARCH/python:$PYTHONPATH

# https://repo.continuum.io/miniconda/Miniconda2-latest-Linux-x86_64.sh
# TODO: Remove need for old .sit_release
RUN curl -O https://repo.continuum.io/miniconda/Miniconda2-4.3.21-Linux-x86_64.sh && \
    echo "b87adda9eb41f3ae6c2999fda739ab61ebb3892e  Miniconda2-4.3.21-Linux-x86_64.sh" | sha1sum --check && \
    /bin/bash Miniconda2-4.3.21-Linux-x86_64.sh -b -p "$CONDA_PREFIX" && \
    conda update -y conda && \
    conda install -y scons cython && \
    conda install -y --channel lcls-rhel7 psana-conda ndarray && \
    mkdir "$REL_PREFIX" && \
    pushd "$REL_PREFIX" && \
      ln -s "$CONDA_PREFIX/lib/python2.7/site-packages/SConsTools/SConstruct.main" SConstruct && \
      export SIT_RELEASE=$(conda list | grep psana-conda | tr -s ' ' | cut -f1-2 -d' ' | tr ' ' '-') && \
      export SIT_USE_CONDA=1 && \
      echo "$SIT_RELEASE" > .sit_release && \
      echo "$CONDA_PREFIX" > .sit_conda_env && \
      git clone https://github.com/lcls-psana/psana.git && \
      git clone https://github.com/lcls-psana/psana_python.git && \
      git clone https://github.com/lcls-psana/python.git && \
      git clone https://github.com/lcls-psana/numpy.git && \
      git clone https://github.com/lcls-psana/PSEnv.git && \
      git clone https://github.com/lcls-psana/PSEvt.git && \
      git clone -b workaround-ambiguity https://github.com/elliottslaughter/PSXtcInput.git && \
      git clone https://github.com/lcls-psana/XtcInput.git && \
      scons includes && \
      scons lib && \
    popd && \
    conda uninstall --force mpich2 && \
    rm Miniconda2-4.3.21-Linux-x86_64.sh
