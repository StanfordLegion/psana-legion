#!/bin/bash

set -e
set -x

# Download model files needed to run
if [[ ! -e 4ngz.pdb ]]; then
    phenix.fetch_pdb 4ngz --mtz
fi

export trial=0
export nproc=1

export TARDATA=output/discovery/dials/r0*/000/out/int*.pickle.tar

rm -rf merge_output
mkdir merge_output

export effective_params="d_min=2.0 \
targlob=$TARDATA \
model=4ngz.pdb \
backend=FS \
scaling.report_ML=True \
pixel_size=0.11 \
nproc=$nproc \
postrefinement.enable=True \
scaling.mtz_file=4ngz.mtz \
scaling.mtz_column_F=f(+) \
min_corr=-1.0 \
output.prefix=merge_output/$trial"

cxi.merge ${effective_params}
cxi.xmerge ${effective_params}
