#!/bin/bash

set -e

rsync -a -P --delete --delete-excluded --exclude '*run*/' --exclude '*res*/' --exclude 'old' "$HOME/psana_legion/psana-legion/psana_legion/" "$SCRATCH/psana_legion_mirror"
