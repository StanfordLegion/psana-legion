/* Copyright 2018 Stanford University
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

#include "native_kernels.h"
#include "native_kernels_helper.h"

#include <stdio.h>
#include <stdlib.h>

void memory_bound_kernel(size_t buffer_size, size_t rounds)
{
  float *buffer = (float *)calloc(buffer_size/sizeof(float), sizeof(float));
  if (!buffer) {
    abort();
  }

  for (size_t round = 0; round < rounds; round++) {
    memory_bound_helper(buffer, buffer_size/sizeof(float));
  }

  free(buffer);
}

void memory_bound_kernel_default()
{
  static size_t buffer_size = 0;
  static size_t rounds = 0;

  if (buffer_size == 0) {
    const char *str = getenv("KERNEL_MEMORY_SIZE");
    if (!str) {
      str = "64";
    }
    long long value = atoll(str); // MB
    if (value <= 0) {
      abort();
    }
    buffer_size = value << 20;
  }

  if (rounds == 0) {
    const char *str = getenv("KERNEL_ROUNDS");
    if (!str) {
      str = "100";
    }
    long long value = atoll(str);
    if (value <= 0) {
      abort();
    }
    rounds = value;
  }

  memory_bound_kernel(buffer_size, rounds);
}
