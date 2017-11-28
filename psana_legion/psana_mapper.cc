/* Copyright 2017 Stanford University
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

#include "psana_mapper.h"

#include "default_mapper.h"

#define MAX_TASKS_IN_FLIGHT 160
#define TASKS_IN_FLIGHT_HYSTERESIS 10

using namespace Legion;
using namespace Legion::Mapping;

class PsanaMapper : public DefaultMapper
{
public:
  PsanaMapper(MapperRuntime *rt, Machine machine, Processor local,
              const char *mapper_name);
  virtual TaskPriority default_policy_select_task_priority(
                                    MapperContext ctx, const Task &task);
  virtual CachedMappingPolicy default_policy_select_task_cache_policy(
                                    MapperContext ctx, const Task &task);
  virtual int default_policy_select_garbage_collection_priority(
                                    MapperContext ctx,
                                    MappingKind kind, Memory memory,
                                    const PhysicalInstance &instance,
                                    bool meets_fill_constraints,bool reduction);
  virtual void configure_context(const MapperContext ctx,
                                 const Task &task,
                                 ContextConfigOutput &output);
  virtual void select_task_options(const MapperContext ctx,
                                   const Task &task,
                                   TaskOptions &output);
  virtual void slice_task(const MapperContext ctx,
                          const Task &task, 
                          const SliceTaskInput &input,
                          SliceTaskOutput &output);
  virtual void map_task(const MapperContext ctx,
                        const Task &task,
                        const MapTaskInput &input,
                        MapTaskOutput &output);
  virtual void select_tasks_to_map(const MapperContext ctx,
                                   const SelectMappingInput &input,
                                   SelectMappingOutput &output);
  virtual void report_profiling(const MapperContext ctx,
                                const Task &task,
                                const TaskProfilingInfo &input);
private:
  void custom_slice_task(const Task &task,
                         const std::vector<Processor> &local,
                         const std::vector<Processor> &remote,
                         const SliceTaskInput &input,
                         SliceTaskOutput &output,
                 std::map<Domain,std::vector<TaskSlice> > &cached_slices) const;
private:
  TaskPriority last_priority;
  unsigned tasks_in_flight, last_tasks_in_flight;
  const unsigned max_tasks_in_flight;
  const unsigned tasks_in_flight_hysteresis;
  size_t task_queue_index;
  MapperEvent defer_select_tasks_to_map;
};

PsanaMapper::PsanaMapper(MapperRuntime *rt, Machine machine, Processor local,
                         const char *mapper_name)
  : DefaultMapper(rt, machine, local, mapper_name)
  , tasks_in_flight(0)
  , last_tasks_in_flight(0)
  , max_tasks_in_flight(MAX_TASKS_IN_FLIGHT)
  , tasks_in_flight_hysteresis(TASKS_IN_FLIGHT_HYSTERESIS)
  , task_queue_index(0)
{
}

TaskPriority
PsanaMapper::default_policy_select_task_priority(
                                    MapperContext ctx, const Task &task)
{
  const char* task_name = task.get_task_name();
  if (strcmp(task_name, "psana_legion.analyze") == 0) {
    // Always enumerate the set of tasks as quickly as possible
    return 1;
  } else if (strcmp(task_name, "psana_legion.analyze_leaf") == 0) {
    // Set initial priority of analysis tasks higher than the continuation
    return 2;
#if 0
    // Rotate priorities on the actual analysis tasks to ensure that
    // we can issue I/O tasks ahead of actual execution
    TaskPriority priority = last_priority++;
    size_t batch = 20 /*window*/ * 8 /*chunksize*/ * 1 /*overcommit*/;
    return batch - priority % batch;
#endif
  }
  return 0;
}


PsanaMapper::CachedMappingPolicy
PsanaMapper::default_policy_select_task_cache_policy(
                                    MapperContext ctx, const Task &task)
{
  // Don't cache task mappings because the mapper will leak instance
  // metadata even if the instances themselves are collected.
  return DEFAULT_CACHE_POLICY_DISABLE;
}



int
PsanaMapper::default_policy_select_garbage_collection_priority(
                                    MapperContext ctx,
                                    MappingKind kind, Memory memory,
                                    const PhysicalInstance &instance,
                                    bool meets_fill_constraints,bool reduction)
{
  // Allow instances to be destroyed on deletion of the region tree.
  return GC_FIRST_PRIORITY;
}

void
PsanaMapper::configure_context(const MapperContext         ctx,
                               const Task&                 task,
                                     ContextConfigOutput&  output)
{
  DefaultMapper::configure_context(ctx, task, output);

  const char* task_name = task.get_task_name();
  if (strcmp(task_name, "psana_legion.analyze_leaf") == 0) {
    output.mutable_priority = true;
  }
}

void
PsanaMapper::select_task_options(const MapperContext    ctx,
                                 const Task&            task,
                                       TaskOptions&     output)
{
  DefaultMapper::select_task_options(ctx, task, output);

  const Task *parent = task.parent_task;
  if (parent && strcmp(parent->get_task_name(), "psana_legion.analyze_leaf") == 0) {
    // Upon blocking, deprioritize the analysis task so that
    // subsequent invokations will occur before any expensive analysis
    output.parent_priority = 0;
  }
}

void
PsanaMapper::slice_task(const MapperContext      ctx,
                        const Task&              task, 
                        const SliceTaskInput&    input,
                              SliceTaskOutput&   output)
{
  Processor::Kind target_kind =
    task.must_epoch_task ? local_proc.kind() : task.target_proc.kind();
  switch (target_kind)
  {
    case Processor::LOC_PROC:
      {
        custom_slice_task(task, local_cpus, remote_cpus, 
                          input, output, cpu_slices_cache);
        break;
      }
    case Processor::TOC_PROC:
      {
        custom_slice_task(task, local_gpus, remote_gpus, 
                           input, output, gpu_slices_cache);
        break;
      }
    case Processor::IO_PROC:
      {
        custom_slice_task(task, local_ios, remote_ios, 
                           input, output, io_slices_cache);
        break;
      }
    case Processor::PY_PROC:
      {
        custom_slice_task(task, local_pys, remote_pys, 
                           input, output, py_slices_cache);
        break;
      }
    case Processor::PROC_SET:
      {
        custom_slice_task(task, local_procsets, remote_procsets, 
                           input, output, procset_slices_cache);
        break;
      }
    case Processor::OMP_PROC:
      {
        custom_slice_task(task, local_omps, remote_omps,
                           input, output, omp_slices_cache);
        break;
      }
    default:
      assert(false); // unimplemented processor kind
  }
}

void PsanaMapper::map_task(const MapperContext ctx,
                           const Task &task,
                           const MapTaskInput &input,
                           MapTaskOutput &output)
{
  DefaultMapper::map_task(ctx, task, input, output);

  const char* task_name = task.get_task_name();
  if (strcmp(task_name, "psana_legion.analyze_leaf") == 0) {
    output.task_prof_requests.add_measurement<Realm::ProfilingMeasurements::OperationStatus>();
  }
}

void PsanaMapper::select_tasks_to_map(const MapperContext ctx,
                                      const SelectMappingInput &input,
                                      SelectMappingOutput &output)
{
  unsigned count = 0;

  // Reset the index whenever tasks in flight may have completed. This
  // ensures that all tasks will (eventually) get scheduled and in the
  // original order.
  if (tasks_in_flight < last_tasks_in_flight)
    task_queue_index = 0;

  assert(task_queue_index <= input.ready_tasks.size());
  std::list<const Task*>::const_iterator it = input.ready_tasks.begin();
  for (size_t i = 0; i < task_queue_index; i++)
    it++;
  for (; (count < max_schedule_count) && (it != input.ready_tasks.end());
       it++, task_queue_index++)
  {
    bool schedule = true;

    const char* task_name = (*it)->get_task_name();
    if (strcmp(task_name, "psana_legion.analyze_leaf") == 0) {
      schedule = tasks_in_flight < max_tasks_in_flight;
      if (schedule) tasks_in_flight++;
    }

    if (schedule) {
      output.map_tasks.insert(*it);
      count++;
    }
  }

  task_queue_index -= count;
  last_tasks_in_flight = tasks_in_flight;

  if (count == 0) {
    if (!defer_select_tasks_to_map.exists()) {
      defer_select_tasks_to_map = runtime->create_mapper_event(ctx);
    }
    output.deferral_event = defer_select_tasks_to_map;
  }
}

void PsanaMapper::report_profiling(const MapperContext ctx,
                                   const Task &task,
                                   const TaskProfilingInfo &input)
{
  const char* task_name = task.get_task_name();
  if (strcmp(task_name, "psana_legion.analyze_leaf") == 0) {
    tasks_in_flight--;
    if ((tasks_in_flight <
         max_tasks_in_flight*(100-tasks_in_flight_hysteresis)/100) &&
        defer_select_tasks_to_map.exists())
    {
      MapperEvent trigger = defer_select_tasks_to_map;
      defer_select_tasks_to_map = MapperEvent();
      runtime->trigger_mapper_event(ctx, trigger);
    }
  } else {
    assert(false); // Shouldn't get profiling responses for any other tasks.
  }
}

void
PsanaMapper::custom_slice_task(const Task &task,
                               const std::vector<Processor> &local,
                               const std::vector<Processor> &remote,
                               const SliceTaskInput &input,
                               SliceTaskOutput &output,
                  std::map<Domain,std::vector<TaskSlice> > &cached_slices) const
{
  // Before we do anything else, see if it is in the cache
  std::map<Domain,std::vector<TaskSlice> >::const_iterator finder = 
    cached_slices.find(input.domain);
  if (finder != cached_slices.end()) {
    output.slices = finder->second;
    return;
  }

  // The two-level decomposition doesn't work so for now do a
  // simple one-level decomposition across all the processors.
  Machine::ProcessorQuery all_procs(machine);
  all_procs.only_kind(local[0].kind());

  // Include only processors NOT on the local node.
  std::set<Processor> local_set(local.begin(), local.end());
  std::vector<Processor> procs;
  for (Machine::ProcessorQuery::iterator it = all_procs.begin(),
         ie = all_procs.end(); it != ie; ++it)
  {
    if (!local_set.count(*it))
      procs.push_back(*it);
  }

  switch (input.domain.get_dim())
  {
    case 1:
      {
        DomainT<1,coord_t> point_space = input.domain;
        Point<1,coord_t> num_blocks(procs.size());
        default_decompose_points<1>(point_space, procs,
              num_blocks, false/*recurse*/,
              stealing_enabled, output.slices);
        break;
      }
    case 2:
      {
        DomainT<2,coord_t> point_space = input.domain;
        Point<2,coord_t> num_blocks =
          default_select_num_blocks<2>(procs.size(), point_space.bounds);
        default_decompose_points<2>(point_space, procs,
            num_blocks, false/*recurse*/,
            stealing_enabled, output.slices);
        break;
      }
    case 3:
      {
        DomainT<3,coord_t> point_space = input.domain;
        Point<3,coord_t> num_blocks =
          default_select_num_blocks<3>(procs.size(), point_space.bounds);
        default_decompose_points<3>(point_space, procs,
            num_blocks, false/*recurse*/,
            stealing_enabled, output.slices);
        break;
      }
    default: // don't support other dimensions right now
      assert(false);
  }

  // Save the result in the cache
  cached_slices[input.domain] = output.slices;
}

static void create_mappers(Machine machine, HighLevelRuntime *runtime, const std::set<Processor> &local_procs)
{
  for (std::set<Processor>::const_iterator it = local_procs.begin();
        it != local_procs.end(); it++)
  {
    PsanaMapper* mapper = new PsanaMapper(runtime->get_mapper_runtime(),
                                          machine, *it, "psana_mapper");
    runtime->replace_default_mapper(mapper, *it);
  }
}

void register_mappers()
{
  Runtime::add_registration_callback(create_mappers);
}
