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
  PLOTFILES="${PLOTFILES} ${COMMA} \"${GNUPLOTFILE}\""
  if [[ "${COMMA}" == "" ]]
  then
    COMMA=","
  fi
done

GNUPLOTSCRIPT=${SOURCEFILE}.gnuplot
rm -f ${GNUPLOTSCRIPT}
echo "#!/usr/bin/gnuplot" >> ${GNUPLOTSCRIPT}
echo "set term postscript" >> ${GNUPLOTSCRIPT}
echo "set output \"${SOURCEFILE}.eps\"" >> ${GNUPLOTSCRIPT}
echo "plot [0:256][1:1.5] ${PLOTFILES} with lines" >> ${GNUPLOTSCRIPT}

