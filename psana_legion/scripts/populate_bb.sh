#!/bin/bash
#SBATCH --nodes=1
#SBATCH --time=00:30:00
#SBATCH --partition=debug
#SBATCH --constraint=haswell
#SBATCH --mail-type=ALL
#SBATCH --account=lcls
#DW persistentdw name=slaughte_data_noepics

export SRC=$SCRATCH/noepics_data/reg
export DEST=$DW_PERSISTENT_STRIPED_slaughte_data_noepics/reg
export THREADS=32

echo "Source $SRC"
echo "Destination $DEST"

echo "Create Directory"
mkdir $DEST
rsync -zr -f"+ */" -f"- *" $SRC/ $DEST/
echo "Begin Transfer"
cd $SRC && find . ! -type d -print0 | xargs -0 -n1 -P$THREADS -I% rsync -a % $DEST/%
echo "Complete Transfer"
du -sh $DEST
ls -lR $DEST
