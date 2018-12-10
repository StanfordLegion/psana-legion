#!/bin/bash

export SRC=/global/project/projectdirs/lcls/d/psdm/cxi/cxid9114
export DEST_ROOT=$SCRATCH/data
export DEST=$DEST_ROOT/reg/d/psdm/cxi/cxid9114
export THREADS=32

echo "Source $SRC"
echo "Destination $DEST"

echo "Create Directory"
mkdir -p $DEST/xtc/smalldata
stripe_medium $DEST/xtc
ln -s $DEST_ROOT/reg/d/psdm/cxi $DEST_ROOT/reg/d/psdm/CXI

echo "Begin Transfer"
rsync -zr -f"+ */" -f"- *" $SRC/calib/ $DEST/calib/
( cd $SRC/calib && find . ! -type d -print0 | xargs -0 -n1 -P$THREADS -I% rsync -a % $DEST/calib/% )
( cd $SRC/xtc && find . -maxdepth 1 -name '*-r0108-*' -print0 | xargs -0 -n1 -P$THREADS -I% rsync -a % $DEST/xtc/% )
( cd $SRC/demo/legion/xtc/smalldata && find . -maxdepth 1 -name '*-r0108-*' -print0 | xargs -0 -n1 -P$THREADS -I% rsync -a % $DEST/xtc/smalldata/% )
echo "Complete Transfer"
du -sh $DEST
ls -lR $DEST
