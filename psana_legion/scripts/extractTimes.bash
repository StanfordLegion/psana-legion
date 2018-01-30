#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
rm -f .tmp1
SOURCE=$1
grep report $SOURCE > .tmp
for i in PY_PROC LOC_PROC IO_PROC
do
  SINK=`echo $SOURCE | sed -e "s/.log//"`.$i.dat
  grep $i .tmp > ${SINK}
  python ${DIR}/statsFromTaskTimes.py $SINK > $SINK.stats
  echo === Stats for $SINK
  cat $SINK.stats
done

