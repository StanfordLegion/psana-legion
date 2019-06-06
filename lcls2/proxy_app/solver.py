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
from legion import task, R, RW
import numpy
from numpy import fft
import os

import data_collector

from phaseret import InitialState, Phaser
from phaseret.generator3D import Projection

###
### Solver
###

# Oversimplified solve on realistic XPP data.
# Somewhat realistic solve on the generated 3D data.
# See user.py for details.


N_POINTS = 64


@task(privileges=[RW], leaf=True)
def generate_data(data):
    cutoff = 2
    spacing = numpy.linspace(-cutoff, cutoff, N_POINTS)

    H, K, L = numpy.meshgrid(spacing, spacing, spacing)

    caffeine_pbd = os.path.join("caffeine.pdb")
    caffeine = Projection.Molecule(caffeine_pbd)

    caffeine_trans = Projection.moltrans(caffeine, H, K, L)
    caffeine_trans_ = fft.ifftshift(caffeine_trans)

    amplitudes = numpy.absolute(caffeine_trans)

    numpy.copyto(data.amplitudes, amplitudes)


@task(privileges=[R, RW], leaf=True)
def preprocess(data_in, data_out):
    print("Preprocessing")
    pass # pretend to build/refine data_out (3D) out of data_in (set of 2D)


@task(privileges=[RW], leaf=True)
def solve_step(data, rank, iteration):
    initial_state = InitialState(data.amplitudes, data.support, data.rho, True)

    if iteration == 0:
        print(f"Initializing rank #{rank}")
        initial_state.generate_support_from_autocorrelation()
        initial_state.generate_random_rho()

    phaser = Phaser(initial_state)
    #phaser.HIO_loop(2, .1)
    phaser.ER_loop(2)
    #phaser.shrink_wrap(.01)

    err_Fourier = phaser.get_Fourier_errs()[-1]
    err_real = phaser.get_real_errs()[-1]
    print(f"Errors: {err_Fourier:.5f}, {err_real:.5f}")


@task(privileges=[RW], replicable=True)
def solve(n_runs):
    n_procs = legion.Tunable.select(legion.Tunable.GLOBAL_PYS).get()
    print(f"Working with {n_procs} processes\n")

    # Allocate data structures.
    n_xpp_events_per_node = 1000
    xpp_event_raw_shape = (2, 3, 6)
    xpp_data = legion.Region.create((n_xpp_events_per_node,) + xpp_event_raw_shape, {'x': legion.uint16})
    legion.fill(xpp_data, 'x', 0)
    xpp_part = legion.Partition.create_equal(xpp_data, [n_procs])

    gen_data_shape = (N_POINTS,) * 3
    data = legion.Region.create(gen_data_shape, {
        'amplitudes': legion.float32,
        'support': legion.uint8,  # should be bool
        'rho': legion.complex64})

    legion.fill(data, 'amplitudes', 0.)
    legion.fill(data, 'support', 0)
    legion.fill(data, 'rho', 0.)

    complete = False
    iteration = 0
    fences = []
    while not complete or iteration < 10:
        if not complete:
            # Obtain the newest copy of the data.
            with legion.MustEpochLaunch([n_procs]):
                for idx in range(n_procs): # legion.IndexLaunch([n_procs]): # FIXME: index launch
                    data_collector.fill_data_region(xpp_part[idx], point=idx)

        # Preprocess data.
        for idx in range(n_procs): # legion.IndexLaunch([n_procs]): # FIXME: index launch
            preprocess(xpp_part, data)

        # Generate data on first run
        if not iteration:
            generate_data(data)

        # Run solver.
        solve_step(data, 0, iteration)

        if not complete:
            # Make sure we don't run more than 2 iterations ahead.
            fences.append(legion.execution_fence(future=True))
            if iteration - 2 >= 0:
                fences[iteration - 2].get()

            # Check that all runs have been read.
            complete = data_collector.get_num_runs_complete() == n_runs

        iteration += 1
