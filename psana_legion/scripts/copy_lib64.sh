#!/bin/bash
# Do this after running build_cori.bash
# These are libs that are not visible to the shifter image, so copy them here where it can see them
#
pushd $HOME/psana_legion/psana-legion/psana_legion
mkdir -p lib64/
cp /opt/cray/ugni/default/lib64/libugni.so.0 lib64/
cp /opt/cray/pe/lib64/libpmi.so.0 lib64/
cp /usr/lib64/libhugetlbfs.so lib64/
cp /opt/cray/pe/lib64/libAtpSigHandler.so.0 lib64/
cp /opt/cray/rca/default/lib64/librca.so.0 lib64/
cp /opt/cray/alps/default/lib64/libalpslli.so.0 lib64/
cp /opt/cray/alps/default/lib64/libalpsutil.so.0 lib64/
cp /opt/cray/udreg/default/lib64/libudreg.so.0 lib64/
cp /opt/cray/wlm_detect/default/lib64/libwlm_detect.so.0 lib64/
# cp /opt/cray/pe/gcc-libs/libgfortran.so.3 lib64/ # FIXME (Elliott): why is this needed?

# Needed for CCTBX to be able to use MPI:
cp /opt/cray/pe/lib64/libmpich_gnu_51.so.3 lib64
cp /opt/cray/xpmem/default/lib64/libxpmem.so.0 lib64/

ls -l lib64
