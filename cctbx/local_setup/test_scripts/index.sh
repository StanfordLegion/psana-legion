#!/bin/bash
START_XTC=$(date +"%s")
EXP=$1
RUN=$2
TRIAL=$3

IN_DIR=$PWD/input
OUT_DIR=$PWD/output

DATA_DIR=$SIT_PSDM_DATA/cxi/cxid9114/xtc

# experiment parameters
RUN_F="$(printf "r%04d" $RUN)"
TRIAL_F="$(printf "%03d" $TRIAL)"

# setup playground
mkdir -p $OUT_DIR/discovery/dials/$RUN_F/$TRIAL_F/out
mkdir -p $OUT_DIR/discovery/dials/$RUN_F/$TRIAL_F/stdout
mkdir -p $OUT_DIR/discovery/dials/$RUN_F/$TRIAL_F/tmp

#run index
# The CCTBX wrapper script uses a Python wrapper that doesn't order PYTHONPATH
# (et al) correctly, so we have to hack all the paths ourselves

### Original version:
#cctbx.xfel.xtc_process \

# Here are the variables that were getting set in the command above.
export PYTHONPATH="$CCTBX_PREFIX/modules/cctbx_project:$CCTBX_PREFIX/modules:$CCTBX_PREFIX/modules/cctbx_project/boost_adaptbx:$CCTBX_PREFIX/modules/cctbx_project/libtbx/pythonpath:$CCTBX_PREFIX/build/lib:$PYTHONPATH"
export LD_LIBRARY_PATH="$CCTBX_PREFIX/build/lib:$PSANA_LEGION_DIR:$LD_LIBRARY_PATH"
export PATH="$CCTBX_PREFIX/build/bin:$PATH"

export LIBTBX_BUILD="$CCTBX_PREFIX/build"
export LIBTBX_PYEXE_BASENAME="python2.7"
export LIBTBX_DISPATCHER_NAME="cctbx.xfel.xtc_process"

### This is what you would run, if we weren't using Legion:
# $CONDA_PREFIX/bin/python -Qnew "$CCTBX_PREFIX/modules/cctbx_project/xfel/command_line/xtc_process.py" \

### Legion version:
export PSANA_MODULE=xtc_process
export PSANA_MAPPER=simple
export PYTHONPATH="$CCTBX_PREFIX/modules/cctbx_project/xfel/command_line:$PYTHONPATH"

export PYTHONUNBUFFERED=1

# psana-legion variables:
export EAGER=1
export LIMIT=10

$PSANA_LEGION_DIR/psana_legion -ll:py 1 -ll:io 1 -ll:csize 6000 \
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
  max_events=1 \
  < /dev/null # Hack: otherwise cctbx freezes in option parsing

END_XTC=$(date +"%s")
ELAPSED=$((END_XTC-START_XTC))
echo TotalElapsed_OneCore $ELAPSED $START_XTC $END_XTC
