FROM centos:centos7
MAINTAINER Elliott Slaughter <slaughter@cs.stanford.edu>

RUN yum clean all && \
    yum -y install bzip2 gcc-c++ git shasum

ENV CONDA_PREFIX /conda
ENV PATH $CONDA_PREFIX/bin:$PATH
ENV SIT_ARCH x86_64-rhel7-gcc48-opt
ENV SIT_RELEASE psana-conda-1.3.9
ENV SIT_ROOT=/reg/g/psdm
ENV SIT_USE_CONDA 1
ENV SIT_REPOS $CONDA_PREFIX

RUN mkdir -p $SIT_ROOT

# https://repo.continuum.io/miniconda/Miniconda2-latest-Linux-x86_64.sh
RUN curl -O https://repo.continuum.io/miniconda/Miniconda2-4.3.21-Linux-x86_64.sh && \
    echo "b87adda9eb41f3ae6c2999fda739ab61ebb3892e  Miniconda2-4.3.21-Linux-x86_64.sh" | sha1sum --check && \
    /bin/bash Miniconda2-4.3.21-Linux-x86_64.sh -b -p "$CONDA_PREFIX" && \
    conda update -y conda && \
    conda install -y scons cython && \
    conda install -y --channel lcls-rhel7 psana-conda ndarray && \
    mkdir "$CONDA_PREFIX/myrel" && \
    pushd "$CONDA_PREFIX/myrel" && \
      ln -s "$CONDA_PREFIX/lib/python2.7/site-packages/SConsTools/SConstruct.main" SConstruct && \
      echo "$SIT_RELEASE" > .sit_release && \
      mkdir -p arch/x86_64-rhel7-gcc48-opt/python && \
      export PYTHONPATH="$PWD/arch/x86_64-rhel7-gcc48-opt/python:" && \
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
    rm Miniconda2-4.3.21-Linux-x86_64.sh
