#!/bin/bash

# To run:
#
# mv -f psana_legion psana_legion_backup
# mkdir psana_legion
# git clone git@github.com:StanfordLegion/psana-legion.git psana_legion/psana-legion
# psana_legion/psana-legion/cctbx/setup/build_from_scratch.sh

set -x

shifterimg -v pull docker:stanfordlegion/cctbx-legion:subprocess-2019-06
shifterimg -v pull docker:stanfordlegion/cctbx-legion:subprocess-2019-06-debug

pushd $HOME/psana_legion
git clone -b subprocess https://gitlab.com/StanfordLegion/legion.git
git clone https://github.com/StanfordLegion/gasnet.git
make -C gasnet CONDUIT=aries

pushd $HOME/psana_legion/psana-legion/psana_legion
./scripts/build_cori_cctbx.sh
./scripts/copy_lib64.sh
./scripts/copy_demo_data.sh
popd

popd
