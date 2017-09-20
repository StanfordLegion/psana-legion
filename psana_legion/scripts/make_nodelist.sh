#!/bin/bash

c="$1"

nodelist="$(scontrol show hostname $SLURM_JOB_NODELIST | tr '\n' ' ')"
first="$(echo "$nodelist" | cut -d' ' -f1)"
rest="$(echo "$nodelist" | cut -d' ' -f2-)"
order="$first"
for elt in $rest; do
  for (( i = 0; i < c; i++ )); do
    order="$order $elt"
  done
done
echo "$order" | tr ' ' '\n' > nodelist.txt
