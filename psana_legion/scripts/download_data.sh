#!/bin/bash

machine="$1"@psexport.slac.stanford.edu
dest_dir="$2"/reg/d/psdm

mkdir -p $dest_dir/xpp/xpptut15/xtc
mkdir -p $dest_dir/xpp/xpptut15/xtc/index
mkdir -p $dest_dir/xpp/xpptut15/xtc/smalldata
rsync -rP $machine':/reg/d/psdm/xpp/xpptut15/xtc/*-r0054-*' $dest_dir/xpp/xpptut15/xtc
rsync -rP $machine':/reg/d/psdm/xpp/xpptut15/xtc/index/*-r0054-*' $dest_dir/xpp/xpptut15/xtc/index
rsync -rP $machine':/reg/d/psdm/xpp/xpptut15/xtc/smalldata/*-r0054-*' $dest_dir/xpp/xpptut15/xtc/smalldata
rsync -rPL $machine':/reg/d/psdm/xpp/xpptut15/calib' $dest_dir/xpp/xpptut15/calib
ln -s xpp $dest_dir/XPP
