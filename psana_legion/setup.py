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

from distutils.core import setup, Extension

module = Extension('native_kernels',
                   sources=['native_kernels_pyext.c'],
                   libraries=['native_kernels_core'],
                   library_dirs=['.'])

setup(name='native_kernels',
      version='1.0',
      description='Sample native kernels for psana-legion.',
      ext_modules=[module])
