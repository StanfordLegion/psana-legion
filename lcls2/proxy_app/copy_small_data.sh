#!/bin/bash

dest=$MEMBERWORK/chm137/mona_small_data

mkdir -p $dest

rsync -rzP $(whoami)@psexport.slac.stanford.edu:/reg/d/psdm/xpp/xpptut15/scratch/mona/xtc2/ $dest
