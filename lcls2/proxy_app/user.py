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

from psana import DataSource
import legion
from legion import task
import os

import native_tasks
import data_collector
import solver

import numpy as np
import os.path
from numpy import fft

import phaseret
from phaseret import InitialState, Phaser
from phaseret.generator3D import Projection


limit = int(os.environ['LIMIT']) if 'LIMIT' in os.environ else None


xtc_dir = os.environ['DATA_DIR']
ds = DataSource('exp=xpptut13:run=1:dir=%s'%(xtc_dir), max_events=limit, det_name='xppcspad')

# FIXME: this crashes if I don't define at least one task here....
@task
def dummy():
    pass

for run in ds.runs():
    # FIXME: must epoch launch
    data_collector.load_run_data(run)

    result = solver.solve()
    print('result of solve is {}'.format(result.get()))

    legion.execution_fence(block=True)
    data_collector.reset_data()


# def create_dataset():
#     cutoff = 2
#     n_points = 64
#     spacing = np.linspace(-cutoff, cutoff, 2*n_points+1)
#     step = cutoff / n_points

#     H, K, L = np.meshgrid(spacing, spacing, spacing)

#     caffeine_pbd = os.path.join("caffeine.pdb")
#     caffeine = Projection.Molecule(caffeine_pbd)

#     caffeine_trans = Projection.moltrans(caffeine, H, K, L)
#     caffeine_trans_ = fft.ifftshift(caffeine_trans)

#     magnitude = np.absolute(caffeine_trans)

#     return magnitude


# def solve(magnitude):
#     magnitude_ = fft.ifftshift(magnitude)
#     initial_state = InitialState(magnitude_, is_ifftshifted=True)

#     phaser = Phaser(initial_state)
#     for k_cycle in range(2):
#         phaser.HIO_loop(10, .1)
#         phaser.ER_loop(10)
#         phaser.shrink_wrap(.01)

#     print("Fourier errors:")
#     print(phaser.get_Fourier_errs()[0])
#     print(phaser.get_Fourier_errs()[-1])

#     print("Real errors:")
#     print(phaser.get_real_errs()[0])
#     print(phaser.get_real_errs()[-1])
