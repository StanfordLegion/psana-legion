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

#include "legion.h"
#include "mappers/default_mapper.h"

#include "native_kernels.h"

using namespace Legion;
using namespace Legion::Mapping;

enum TaskIDs {
  TOP_LEVEL_TASK_ID = 1,
  MEMORY_BOUND_TASK_ID = 2,
  DUMMY_TASK_ID = 3,
};

struct MemoryBoundArgs {
  size_t buffer_size; // bytes
  size_t rounds;
};

void memory_bound_task(const Task *task,
                       const std::vector<PhysicalRegion> &regions,
                       Context ctx, Runtime *runtime)
{
  assert(task->arglen == sizeof(MemoryBoundArgs));
  MemoryBoundArgs args = *reinterpret_cast<const MemoryBoundArgs*>(task->args);

  memory_bound_kernel(args.buffer_size, args.rounds);
}

void dummy_task(const Task *task,
                const std::vector<PhysicalRegion> &regions,
                Context ctx, Runtime *runtime)
{
}

void top_level_task(const Task *task,
                    const std::vector<PhysicalRegion> &regions,
                    Context ctx, Runtime *runtime)
{
  long long buffer_size = 64; // MB
  long long rounds = 20;
  long long iterations = 100;
  const InputArgs &args = Runtime::get_input_args();
  for (int i = 1; i < args.argc; i++) {
    // Skip any legion runtime configuration parameters
    if (!strcmp(args.argv[i], "-size")) {
      buffer_size = atoll(args.argv[++i]);
      continue;
    }
    if (!strcmp(args.argv[i], "-rounds")) {
      rounds = atoll(args.argv[++i]);
      continue;
    }
    if (!strcmp(args.argv[i], "-iter")) {
      iterations = atoll(args.argv[++i]);
      continue;
    }
  }
  assert(buffer_size >= 0);
  assert(rounds >= 0);
  assert(iterations >= 0);

  long long buffer_size_bytes = buffer_size << 20; // MB -> bytes

  size_t num_procs = runtime->select_tunable_value(
    ctx, DefaultMapper::DEFAULT_TUNABLE_GLOBAL_CPUS).get_result<size_t>();

  printf("Kernel using %lld MB buffer for %lld rounds\n",
         buffer_size, rounds);
  printf("Running %lld iterations on %lu processors...\n",
         iterations, num_procs);

  unsigned long long start_ts = Realm::Clock::current_time_in_microseconds();

  {
    MemoryBoundArgs args;
    args.buffer_size = buffer_size_bytes; // MB -> bytes
    args.rounds = rounds;

    Rect<1> launch_domain(0, num_procs-1);
    for (int i = 0; i < iterations; i++) {
      IndexTaskLauncher launcher(MEMORY_BOUND_TASK_ID, launch_domain,
                                 TaskArgument(&args, sizeof(args)),
                                 ArgumentMap());
      runtime->execute_index_space(ctx, launcher);
    }
  }

  runtime->issue_execution_fence(ctx);

  {
    TaskLauncher launcher(DUMMY_TASK_ID, TaskArgument());
    Future f = runtime->execute_task(ctx, launcher);
    f.get_void_result();
  }

  unsigned long long stop_ts = Realm::Clock::current_time_in_microseconds();
  double elapsed = (stop_ts - start_ts)/1e6;

  long long total_tasks = iterations*num_procs;
  long long total_read = total_tasks*rounds*buffer_size;

  printf("Elapsed time: %e seconds\n", elapsed);
  printf("Throughput: %e tasks/s\n", total_tasks/elapsed);
  printf("Time per task: %e seconds\n", elapsed/total_tasks);
  printf("Memory bandwidth achieved: %e GB/s\n", total_read/elapsed/(1<<10));
  printf("Total tasks: %lld\n", total_tasks);
  printf("Total data read: %lld MB\n", total_read);
}

int main(int argc, char **argv)
{
  {
    TaskVariantRegistrar registrar(TOP_LEVEL_TASK_ID, "top_level_task");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<top_level_task>(registrar, "top_level_task");
  }

  {
    TaskVariantRegistrar registrar(MEMORY_BOUND_TASK_ID, "memory_bound_task");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<memory_bound_task>(registrar, "memory_bound_task");
  }

  {
    TaskVariantRegistrar registrar(DUMMY_TASK_ID, "dummy_task");
    registrar.add_constraint(ProcessorConstraint(Processor::LOC_PROC));
    Runtime::preregister_task_variant<dummy_task>(registrar, "dummy_task");
  }

  Runtime::set_top_level_task_id(TOP_LEVEL_TASK_ID);

  return Runtime::start(argc, argv);
}
