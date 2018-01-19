#!/bin/bash

START_XTC=$(date +"%s")
EXP=$1
RUN=$2
TRIAL=$3

# FIXME (Elliott): Does the trial number matter????

source $CCTBX_PREFIX/build/setpaths.sh

DATA_DIR=$SIT_PSDM_DATA/cxi/cxid9114/xtc

# experiment parameters
RUN_F="$(printf "r%04d" $RUN)"
TRIAL_F="$(printf "%03d" $TRIAL)"

# setup playground
mkdir -p $OUT_DIR/discovery/dials/$RUN_F/$TRIAL_F/out
mkdir -p $OUT_DIR/discovery/dials/$RUN_F/$TRIAL_F/stdout
mkdir -p $OUT_DIR/discovery/dials/$RUN_F/$TRIAL_F/tmp

#run index
cctbx.xfel.xtc_process \
  input.experiment=$EXP \
  input.run_num=$RUN \
  output.logging_dir=$OUT_DIR/discovery/dials/$RUN_F/$TRIAL_F/stdout \
  output.output_dir=$OUT_DIR/discovery/dials/$RUN_F/$TRIAL_F/out \
  format.cbf.invalid_pixel_mask=$IN_DIR/mask_ld91.pickle \
  $IN_DIR/process_batch.phil \
  dump_indexed=False \
  output.tmp_output_dir=$OUT_DIR/discovery/dials/$RUN_F/$TRIAL_F/tmp \
  input.reference_geometry=$IN_DIR/geom_ld91.json \
  input.xtc_dir=$DATA_DIR \
  max_events=$LIMIT # Hack

END_XTC=$(date +"%s")
ELAPSED=$((END_XTC-START_XTC))
echo TotalElapsed_OneCore $ELAPSED $START_XTC $END_XTC
