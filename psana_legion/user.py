#!/usr/bin/env python

# Copyright 2017 Stanford University
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

import psana
import psana_legion

ds = psana_legion.ds # FIXME: Allow this to be declared here
det = psana.Detector('cspad', ds.env())

def analyze(event):
    print('analyze', event)
    raw = det.raw(event)
    calib = det.calib(event) # Calibrate the data
    print(raw.sum(), calib.sum())

def filter(event):
    return True

psana_legion.start(analyze, filter)
