#!/bin/bash

if [[ "$1" == "" ]]
then
  echo must specify a sourec file, output from mapper_stats_from_log
  exit 1
fi

SOURCEFILE=$1

for PROC in IO_PROC PY_PROC
do
  GNUPLOTFILE=${SOURCEFILE}.${PROC}.dat
  rm -f ${GNUPLOTFILE}
  grep balance ${SOURCEFILE} | grep -v "Comput" | grep -v key | grep ${PROC} | sed -e "s/balance ${PROC} //" | sort -n >> ${GNUPLOTFILE}
  echo "" >> ${GNUPLOTFILE}
  echo "" >> ${GNUPLOTFILE}
  grep "random " ${SOURCEFILE} | grep -v "Comput" | grep -v key | grep ${PROC} | sed -e "s/random ${PROC} //" | sort -n >> ${GNUPLOTFILE}
done
