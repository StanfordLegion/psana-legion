#!/bin/bash

ls $SCRATCH/cori-cctbx.subprocess/output_*/discovery/dials/r0108/000/stdout/log_rank0000.out | sort -t_ -k5,5 -k7,7 -k9,9 -k11,11 -k3,3 -n | xargs grep '^Elapsed' | tr _ '\t' | tr / '\t' | sed 's/[.]/\t/2' | tr ' ' '\t' | cut -f 8,11,13,15,17,27
