#!/bin/bash
DIR="$(dirname "${BASH_SOURCE[0]}")"

if [[ "$1" == "" ]]
then
  must specify a directory that holds a set of log files
  exit 1
fi

DATADIR=$1

cat ${DATADIR}/*.log | python ${DIR}/mapper_stats_from_log.py | grep balance 

