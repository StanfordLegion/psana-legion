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

#include "native_kernels_tasks.h"

#include "native_kernels.h"

#include "legion.h"

#include <cstdint>
#include <cinttypes>

using namespace Legion;

void memory_bound_task(const Task *task,
                       const std::vector<PhysicalRegion> &regions,
                       Context ctx, Runtime *runtime)
{
  memory_bound_kernel_default();
}

void cache_bound_task(const Task *task,
                      const std::vector<PhysicalRegion> &regions,
                      Context ctx, Runtime *runtime)
{
  cache_bound_kernel_default();
}

int64_t sum_task(const Task *task,
              const std::vector<PhysicalRegion> &regions,
              Context ctx, Runtime *runtime)
{
  assert(regions.size() == 1);

  const FieldAccessor<READ_ONLY, int16_t, 3> x(regions[0], X_FIELD_ID);

  Rect<3> rect = runtime->get_index_space_domain(ctx,
                  regions[0].get_logical_region().get_index_space());

  int64_t sum = 0;
  for (PointInRectIterator<3> p(rect); p(); p++) {
    sum += x[*p];
  }
  // printf("sum is %" PRId64 "\n", sum);
  return sum;
}

#ifdef USE_CUDA
int64_t gpu_sum_task(const Task *task,
                     const std::vector<PhysicalRegion> &regions,
                     Context ctx, Runtime *runtime);
#endif

void preregister_native_kernels_tasks(int memory_bound_task_id,
                                   int cache_bound_task_id,
                                   int sum_task_id)
{
  {
    TaskVariantRegistrar registrar(memory_bound_task_id, "memory_bound_task");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<memory_bound_task>(registrar, "memory_bound_task");
  }

  {
    TaskVariantRegistrar registrar(cache_bound_task_id, "cache_bound_task");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<cache_bound_task>(registrar, "cache_bound_task");
  }

  {
    TaskVariantRegistrar registrar(sum_task_id, "sum_task");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<int64_t, sum_task>(registrar, "sum_task");
  }

#ifdef USE_CUDA
  {
    TaskVariantRegistrar registrar(sum_task_id, "sum_task");
    registrar.add_constraint(ProcessorConstraint(Processor::TOC_PROC));
    Runtime::preregister_task_variant<int64_t, gpu_sum_task>(registrar, "sum_task");
  }
#endif
}

void register_native_kernels_tasks(int memory_bound_task_id,
                                   int cache_bound_task_id,
                                   int sum_task_id)
{
  Runtime *runtime = Runtime::get_runtime();

  {
    TaskVariantRegistrar registrar(memory_bound_task_id, "memory_bound_task");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    runtime->register_task_variant<memory_bound_task>(registrar);
    runtime->attach_name(memory_bound_task_id, "memory_bound_task");
  }

  {
    TaskVariantRegistrar registrar(cache_bound_task_id, "cache_bound_task");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    runtime->register_task_variant<cache_bound_task>(registrar);
    runtime->attach_name(cache_bound_task_id, "cache_bound_task");
  }

  {
    TaskVariantRegistrar registrar(sum_task_id, "sum_task");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    runtime->register_task_variant<int64_t, sum_task>(registrar);
    runtime->attach_name(sum_task_id, "sum_task");
  }

#ifdef USE_CUDA
  {
    TaskVariantRegistrar registrar(sum_task_id, "sum_task");
    registrar.add_constraint(ProcessorConstraint(Processor::TOC_PROC));
    runtime->register_task_variant<int64_t, gpu_sum_task>(registrar);
  }
#endif
}
