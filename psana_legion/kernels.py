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

import os
import numpy

def nop_kernel():
    pass

_memory_size = int(os.environ.get('KERNEL_MEMORY_SIZE', 64)) # MB

def make_memory_bound_kernel(rounds):
    def kernel():
        x = numpy.zeros(_memory_size << 17) # allocate array of doubles
        for i in range(rounds):
            numpy.add(x, 1, out=x)
    return kernel
