#!/bin/bash
#
# Make a sbatch script for Cori
#
# Arguments: mapper, nodes
#

if [[ "$1" == "" ]]
then
  echo Please specify mapper \(lifeline, task_pool, random\)
  exit 1
fi

MAPPER=$1

if [[ "$2" == "" ]]
then
  echo Please specify number of nodes
  exit 1
fi

NODES=$2

SBATCH=sbatch_${MAPPER}_${NODES}
rm -f ${SBATCH}
cat sbatch_cori.template | sed -e "s/_MAPPER_/${MAPPER}/g" | sed -e "s/_NODES_/${NODES}/g" > ${SBATCH}
ls -l ${SBATCH}

