#!/bin/bash

ls $SCRATCH/cori-cctbx.subprocess/output_*/discovery/dials/r0108/000/stdout/log_rank0000.out | sort -t_ -k2,2 -k5n,5n -k7n,7n -k9n,9n -k11n,11n -k3n,3n | xargs grep '^Elapsed' | tr _ '\t' | tr / '\t' | sed 's/[.]/\t/2' | tr ' ' '\t' | cut -f 8,11,13,15,17,27
