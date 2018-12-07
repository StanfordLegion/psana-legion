#!/bin/bash

ls rax_*.log | sort -t_ -k4,4 -k6,6 -k8,8 -n | xargs grep '^Elapsed' | tr _ '\t' | sed 's/[.]/\t/1' | tr ' ' '\t' | cut -d$'\t' -f 4,6,8,11
