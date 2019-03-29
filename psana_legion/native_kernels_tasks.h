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

#ifndef __NATIVE_KERNELS_TASKS_H__
#define __NATIVE_KERNELS_TASKS_H__

#ifdef __cplusplus
extern "C" {
#endif

void preregister_native_kernels_tasks(int memory_bound_task_id,
				      int cache_bound_task_id,
				      int sum_task_id);

void register_native_kernels_tasks(int memory_bound_task_id,
                                   int cache_bound_task_id,
                                   int sum_task_id);

#ifdef __cplusplus
}
#endif

#endif // __NATIVE_KERNELS_TASKS_H__
