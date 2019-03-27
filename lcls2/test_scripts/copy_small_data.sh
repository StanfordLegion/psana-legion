#!/bin/bash

dest=$MEMBERWORK/chm137/mona_small_data

mkdir -p $dest

scp $(whoami)@psexport.slac.stanford.edu:/reg/d/psdm/xpp/xpptut15/scratch/mona/xtc2/smalldata/*.xtc2 $dest
