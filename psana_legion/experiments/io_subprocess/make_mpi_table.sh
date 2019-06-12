#!/bin/bash

ls rax_*.log | sort -t_ -k3,3 -k5,5 -n | xargs grep '^Elapsed' | tr _ '\t' | sed 's/[.]/\t\t\t/1' | tr ' ' '\t' | cut -f 3,5,6,7,10
