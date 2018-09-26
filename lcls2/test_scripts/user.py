#!/usr/bin/env python

# Copyright 2018 Stanford University
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

from __future__ import print_function

import os
import psana
from psana import DataSource

limit = int(os.environ['LIMIT']) if 'LIMIT' in os.environ else None

# To test on 'real' bigdata:
# xtc_dir = "/reg/d/psdm/xpp/xpptut15/scratch/mona/test"
xtc_dir = os.path.join(os.getcwd(),'.tmp')
ds = DataSource('exp=xpptut13:run=1:dir=%s'%(xtc_dir), max_events=limit, det_name='xppcspad')

# FIXME: For some reason we need this loop, even though we're not going to do any analysis inside.
for run in ds.runs():
    det = ds.Detector(ds.det_name)

def event_fn(event):
    print('Analyzing event', event)
ds.analyze(event_fn=event_fn)
