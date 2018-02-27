# Quickstart

```
git clone -b python https://github.com/StanfordLegion/legion.git
git clone https://github.com/StanfordLegion/psana-legion.git

export LG_RT_DIR=$PWD/legion/runtime
export PSANA_LEGION_DIR=$PWD/psana-legion/psana_legion
export SIT_PSDM_DATA=$PWD/data/d/psdm

# Please download a sample data set (such as cxid9114 run 108) and
# place it in ./data

# Download Phenix and place the installer at
# ./psana_legion/cctbx/phenix-installer.tar.gz

cd psana_legion/cctbx
./build_from_scratch.sh
cd test_scripts
./run.sh
```
