#!/bin/bash
clear
for i in 1 2 3 4
do
  grep "proc 1d0000000000000$i:" debug.log > debug.$i.log
  echo ===
  echo === $i
  echo ===
  tail debug.$i.log
done
