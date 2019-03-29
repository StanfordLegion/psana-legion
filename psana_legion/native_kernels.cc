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

#include "native_kernels.h"
#include "native_kernels_helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void memory_bound_kernel(size_t buffer_size, size_t rounds)
{
  thread_local float *buffer = NULL;
  if (!buffer) {
    buffer = (float *)malloc(buffer_size);
    if (!buffer) abort();
  }

  memset(buffer, 0, buffer_size);

  for (size_t round = 0; round < rounds; round++) {
    memory_bound_helper(buffer, buffer_size/sizeof(float));
  }

  // Just leak the memory...
}

void memory_bound_kernel_default()
{
  static volatile size_t buffer_size = 0;
  static volatile size_t rounds = 0;
  static volatile size_t dop = 0;

  // Load globals into local variables.
  size_t new_buffer_size = buffer_size;
  size_t new_rounds = rounds;
  size_t new_dop = dop;

  if (new_buffer_size == 0 || new_rounds == 0 || new_dop == 0) {
    if (new_buffer_size == 0) {
      const char *str = getenv("KERNEL_MEMORY_SIZE");
      if (!str) {
        str = "64";
      }
      long long value = atoll(str); // MB
      if (value <= 0) {
        abort();
      }
      new_buffer_size = value << 20;
    }

    if (new_rounds == 0) {
      const char *str = getenv("KERNEL_ROUNDS");
      if (!str) {
        str = "100";
      }
      long long value = atoll(str);
      if (value <= 0) {
        abort();
      }
      new_rounds = value;
    }

    if (new_dop == 0) {
      const char *str = getenv("KERNEL_DOP");
      if (!str) {
        str = "1";
      }
      long long value = atoll(str); // MB
      if (value <= 0) {
        abort();
      }
      new_dop = value;

    }

    // Divide out parallelism to get correct buffer size.
    new_buffer_size = new_buffer_size / new_dop;

    // Store back into global variables.
    // It's ok that this is racy because everyone will write the same result.
    buffer_size = new_buffer_size;
    rounds = new_rounds;
    dop = new_dop;
  }

  memory_bound_kernel(buffer_size, rounds);
}

void cache_bound_kernel_default()
{
  static volatile size_t buffer_size = 0;
  static volatile size_t rounds = 0;
  static volatile size_t dop = 0;

  // Load globals into local variables.
  size_t new_buffer_size = buffer_size;
  size_t new_rounds = rounds;
  size_t new_dop = dop;

  if (new_buffer_size == 0 || new_rounds == 0 || new_dop == 0) {
    if (new_buffer_size == 0) {
      const char *str = getenv("KERNEL_MEMORY_SIZE");
      if (!str) {
        str = "64";
      }
      long long value = atoll(str); // bytes
      if (value/sizeof(float) <= 0 || value%sizeof(float) != 0) {
        abort();
      }
      new_buffer_size = value;
    }

    if (new_rounds == 0) {
      const char *str = getenv("KERNEL_ROUNDS");
      if (!str) {
        str = "100";
      }
      long long value = atoll(str);
      if (value <= 0) {
        abort();
      }
      new_rounds = value;
    }

    if (new_dop == 0) {
      const char *str = getenv("KERNEL_DOP");
      if (!str) {
        str = "1";
      }
      long long value = atoll(str); // MB
      if (value <= 0) {
        abort();
      }
      new_dop = value;

    }

    // Divide out parallelism to get correct buffer size.
    new_buffer_size = new_buffer_size / new_dop;

    // Store back into global variables.
    // It's ok that this is racy because everyone will write the same result.
    buffer_size = new_buffer_size;
    rounds = new_rounds;
    dop = new_dop;
  }

  memory_bound_kernel(buffer_size, rounds);
}
