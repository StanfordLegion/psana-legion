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

import numpy

def nop_kernel():
    pass

def make_memory_bound_kernel(rounds):
    def kernel():
        x = numpy.zeros(1<<23) # allocate 64 MB array (larger than L3 cache)
        for i in range(rounds):
            numpy.add(x, 1, out=x)
    return kernel