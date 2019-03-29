/* Copyright 2019 Stanford University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Important: DO NOT include legion.h from this file; it is called
// from both Legion and MPI

#include "native_kernels_helper.h"

// This is separated into its own file so that the compiler doesn't
// inline it and optimize away the computation.

#if defined(__AVX__) || defined(__AVX2__) || defined(__AVX512F__)
__attribute__((target_clones("arch=knl","arch=haswell","default")))
#endif
void memory_bound_helper(float * RESTRICT buffer, size_t count)
{
  for (size_t i = 0; i < count; i++) {
    buffer[i] += 1.0;
  }
}
