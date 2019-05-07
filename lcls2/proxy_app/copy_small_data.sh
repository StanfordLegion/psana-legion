#!/bin/bash

if [[ $(hostname --fqdn) = *"summit"* ]]; then
    dest=$MEMBERWORK/chm137/mona_small_data
elif [[ $(hostname) = "cori"* ]]; then
    dest=$SCRATCH/mona_small_data
else
    echo "Unable to auto-detect the machine"
    exit 1
fi

mkdir -p $dest

rsync -rzP ${PSUSER:-$USER}@psexport.slac.stanford.edu:/reg/d/psdm/xpp/xpptut15/scratch/mona/xtc2/ $dest
