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

#ifndef __NATIVE_KERNELS_H__
#define __NATIVE_KERNELS_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void memory_bound_kernel(size_t buffer_size, size_t rounds);

void memory_bound_kernel_default(void);

#ifdef __cplusplus
}
#endif

#endif // __NATIVE_KERNELS_H__
