#!/bin/bash

ls rax_*.log | sort -t_ -k3,3 -k5,5 -k7,7 -k9,9 -n | xargs grep '^Elapsed' | tr _ '\t' | sed 's/[.]/\t/1' | tr ' ' '\t' | cut -d$'\t' -f 3,5,7,9,12
