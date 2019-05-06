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

import data_collector

from phaseret import InitialState, Phaser

###
### Solver
###

# Oversimplified solve on realistic XPP data.
# Somewhat realistic solve on the generated 3D data.
# See user.py for details.


@task(privileges=[RW])
def preprocess(data):
    pass # do nothing in the preprocess phase for now


@task(privileges=[R])
def solve_xpp_step(data):
    return data.x.sum()


@task(privileges=[RW], replicable=True)
def solve_xpp(n_procs):
    # Allocate data structures.
    n_events_per_node = 1000
    event_raw_shape = (2, 3, 6)
    data = legion.Region.create((n_events_per_node,) + event_raw_shape, {'x': legion.uint16})
    legion.fill(data, 'x', 0)
    part = legion.Partition.create_equal(data, [n_procs])

    iteration = 0
    overall_answer = 0
    while iteration < 2:
        # This used to be while overall_answer == 0,
        # assuming that all (currently) 10 events would
        # arrive at the same time but it happened to me once that
        # 1 event got caught before the others.

        # Obtain the newest copy of the data.
        # FIXME: must epoch launch
        for idx in range(n_procs): # legion.IndexLaunch([n_procs]): # FIXME: index launch
            data_collector.fill_xpp_data_region(part[idx])

        # Preprocess data.
        for idx in range(n_procs): # legion.IndexLaunch([n_procs]): # FIXME: index launch
            preprocess(part[idx])

        # Run solver.
        futures = []
        for idx in range(n_procs): # legion.IndexLaunch([n_procs]): # FIXME: index launch
            futures.append(solve_xpp_step(part[idx]))
        overall_answer = 0
        for future in futures:
            overall_answer += future.get()
        print('XPP: iteration {} result of solve is {}'.format(iteration, overall_answer))
        iteration += 1

    return overall_answer


@task(privileges=[R])
def solve_gen_step(data):
    initial_state = InitialState(data.magnitude)

    phaser = Phaser(initial_state)
    for k_cycle in range(2):
        phaser.HIO_loop(10, .1)
        phaser.ER_loop(10)
        phaser.shrink_wrap(.01)

    print("Fourier errors:")
    print(phaser.get_Fourier_errs()[0])
    print(phaser.get_Fourier_errs()[-1])

    print("Real errors:")
    print(phaser.get_real_errs()[0])
    print(phaser.get_real_errs()[-1])

    return "Done"


@task(privileges=[RW], replicable=True)
def solve_gen(solve_idx=0):
    """Solve the phase problem for the generated data.

    Since we might want to perform several reconstructions in parallel,
    the param solve_idx can be used to differentiate the reconstructions.
    """
    # Allocate data structures.
    data_shape = (2*64 + 1,) * 3
    data = legion.Region.create(data_shape, {'magnitude': legion.float64})
    legion.fill(data, 'magnitude', 0)

    iteration = 0
    overall_answer = 0
    while overall_answer == 0:
        # Obtain the newest copy of the data.
        # FIXME: must epoch launch
        data_collector.fill_gen_data_region(data)

        # Preprocess data.
        preprocess(data)

        # Run solver.
        future = solve_gen_step(data)

        overall_answer = future.get()

        print('Gen-{}: iteration {} result of solve is {}'.format(
            solve_idx, iteration, overall_answer))
        iteration += 1
    return overall_answer
