#!/bin/bash
#BSUB -P CSC103SUMMITDEV
#BSUB -W 0:30
#BSUB -nnodes 1
#BSUB -o lsf-%J.out
#BSUB -e lsf-%J.err
#BSUB -N

export LIMIT=1

export all_proxy=socks://proxy.ccs.ornl.gov:3128/
export ftp_proxy=ftp://proxy.ccs.ornl.gov:3128/
export http_proxy=http://proxy.ccs.ornl.gov:3128/
export https_proxy=https://proxy.ccs.ornl.gov:3128/
export no_proxy='localhost,127.0.0.0/8,*.ccs.ornl.gov,*.ncrc.gov'

jsrun -n 1 python simple_user.py
