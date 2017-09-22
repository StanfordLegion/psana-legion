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

using namespace Legion;
using namespace Legion::Mapping;

class PsanaMapper : public DefaultMapper
{
public:
  PsanaMapper(MapperRuntime *rt, Machine machine, Processor local,
              const char *mapper_name);
  virtual void slice_task(const MapperContext ctx,
                          const Task &task, 
                          const SliceTaskInput &input,
                          SliceTaskOutput &output);
private:
  void custom_slice_task(const Task &task,
                         const std::vector<Processor> &local,
                         const std::vector<Processor> &remote,
                         const SliceTaskInput &input,
                         SliceTaskOutput &output,
                 std::map<Domain,std::vector<TaskSlice> > &cached_slices) const;
};

PsanaMapper::PsanaMapper(MapperRuntime *rt, Machine machine, Processor local,
                         const char *mapper_name)
  : DefaultMapper(rt, machine, local, mapper_name)
{
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
        Rect<1> point_rect = input.domain.get_rect<1>();
        Point<1> num_blocks(procs.size());
        default_decompose_points<1>(point_rect, procs,
              num_blocks, false/*recurse*/,
              stealing_enabled, output.slices);
        break;
      }
    case 2:
      {
        Rect<2> point_rect = input.domain.get_rect<2>();
        Point<2> num_blocks =
          default_select_num_blocks<2>(procs.size(), point_rect);
        default_decompose_points<2>(point_rect, procs,
            num_blocks, false/*recurse*/,
            stealing_enabled, output.slices);
        break;
      }
    case 3:
      {
        Rect<3> point_rect = input.domain.get_rect<3>();
        Point<3> num_blocks =
          default_select_num_blocks<3>(procs.size(), point_rect);
        default_decompose_points<3>(point_rect, procs,
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
  HighLevelRuntime::add_registration_callback(create_mappers);
}
