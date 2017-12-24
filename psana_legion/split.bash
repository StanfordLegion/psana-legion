#!/bin/bash
egrep "node 1d0000 proc .*:" run.log > run0.log
egrep "node 1d0001 proc .*:" run.log > run1.log
egrep "node 1d0002 proc .*:" run.log > run2.log
