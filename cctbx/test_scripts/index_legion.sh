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
# The CCTBX wrapper script uses a Python wrapper that doesn't order PYTHONPATH
# (et al) correctly, so we have to hack all the paths ourselves

### Original version:
#cctbx.xfel.xtc_process \

# Here are the variables that were getting set in the command above.
export PSANA_LEGION_DIR=$HOST_PSANA_DIR
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
export PYTHONPATH="$CCTBX_PREFIX/modules/cctbx_project/xfel/command_line:$PYTHONPATH"

### Important: Put the EXTERNAL copy of psana_legion on path before the INTERNAL copy
external_psana_dir=$PSANA_LEGION_DIR
export PYTHONPATH="$external_psana_dir:$PYTHONPATH"
export LD_LIBRARY_PATH="$external_psana_dir:$external_psana_dir/lib64:$LD_LIBRARY_PATH"

# FIXME: This seems to be necessary (otherwise Python can't find __future__ ???)
export PYTHONHOME="$CONDA_PREFIX"

# IMPORTANT: Python searches the current directory by default, and this can get very, very slow at high node counts
# Work around it by cd'ing to a known local directory before running
# (Otherwise the current directory shouldn't matter)
cd $external_psana_dir

# Similarly, set HOME to be absolutely sure that nothing is touching the real home directory.
export HOME=$external_psana_dir

$external_psana_dir/psana_legion "${@:4}" \
  input.experiment=$EXP \
  input.run_num=$RUN \
  output.logging_dir=$OUT_DIR/discovery/dials/$RUN_F/$TRIAL_F/stdout \
  output.output_dir=$OUT_DIR/discovery/dials/$RUN_F/$TRIAL_F/out \
  format.cbf.invalid_pixel_mask=$IN_DIR/mask_ld91.pickle \
  $IN_DIR/process_batch.phil \
  dump_indexed=False \
  output.tmp_output_dir=$OUT_DIR/discovery/dials/$RUN_F/$TRIAL_F/tmp \
  input.reference_geometry=$IN_DIR/geom_ld91.json \
  input.xtc_dir=$DATA_DIR # \
  # max_events=$LIMIT # Hack (FIXME: This doesn't actually do anything in Legion mode)

# pid=$!
# sleep 15m
# pstree -u $(whoami) -p > "$OUT_DIR/backtrace/pstree_${SLURM_PROCID}_$(hostname).log"
# sleep 15
# (
#     unset PYTHONHOME
#     unset PYTHONPATH
#     unset LD_LIBRARY_PATH
#     export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
#     gdb -p $pid -batch -quiet -ex "thread apply all bt" 2>&1 > "$OUT_DIR/backtrace/bt_${SLURM_PROCID}_$(hostname)_$pid.log"
# )
# wait

END_XTC=$(date +"%s")
ELAPSED=$((END_XTC-START_XTC))
echo TotalElapsed_OneCore $ELAPSED $START_XTC $END_XTC
