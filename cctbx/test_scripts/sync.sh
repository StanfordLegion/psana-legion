#!/bin/bash

set -e

rsync --exclude '*run*/' --exclude '*res*/' -r -P "$HOME/psana_legion/psana-legion/psana_legion/" "$SCRATCH/psana_legion_mirror"
