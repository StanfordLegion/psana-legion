#!/usr/bin/env python

# Copyright 2019 Stanford University
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

import legion
from legion import task, RW
import numpy
from numpy import fft
import os
import threading

from phaseret.generator3D import Projection

###
### Data Loading
###

# Load realistic xpp data for the collecting part.
# Create a 3D diffraction image for the phasing part.
# See user.py for details.


data_store = []
n_events_ready = 0
n_events_used = 0
data_lock = threading.Lock()


def load_event_data(event, det):
    global n_events_ready
    raw = det.raw.raw(event)
    with data_lock:
        data_store.append((event, raw))
        n_events_ready += 1


def load_run_data(run):
    det = run.Detector('xppcspad')

    # Hack: psana tries to register top-level task when not in script mode
    old_is_script = legion.is_script
    legion.is_script = True
    run.analyze(event_fn=load_event_data, det=det)
    legion.is_script = old_is_script


def reset_data():
    global data_store, n_events_ready, n_events_used
    with data_lock:
        data_store = []
        n_events_ready = 0
        n_events_used = 0


@task(privileges=[RW])
def fill_xpp_data_region(data):
    global data_store, n_events_used
    with data_lock:
        raw, used, ready = data_store, n_events_used, n_events_ready
        data_store = []
        n_events_used = ready

    for idx in range(used, ready):
        numpy.copyto(data.x[idx,:,:,:], raw[idx - used][1], casting='no')


@task(privileges=[RW])
def fill_gen_data_region(data):
    cutoff = 2
    n_points = 64
    spacing = numpy.linspace(-cutoff, cutoff, 2*n_points+1)
    step = cutoff / n_points

    H, K, L = numpy.meshgrid(spacing, spacing, spacing)

    caffeine_pbd = os.path.join("caffeine.pdb")
    caffeine = Projection.Molecule(caffeine_pbd)

    caffeine_trans = Projection.moltrans(caffeine, H, K, L)
    caffeine_trans_ = fft.ifftshift(caffeine_trans)

    magnitude = numpy.absolute(caffeine_trans)

    numpy.copyto(data.magnitude, magnitude)
