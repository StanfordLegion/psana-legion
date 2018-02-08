/* Copyright 2018 Stanford University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS"BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "random_mapper.hpp"
#include "default_mapper.h"


static const int TASKS_PER_SLICE = 1;


class RandomMapper : public DefaultMapper
{
public:
  RandomMapper(MapperRuntime *rt,
                 Machine machine, Processor local,
                 const char *mapper_name,
                 std::vector<Processor>* procs_list,
                 std::vector<Memory>* sysmems_list,
                 std::map<Memory, std::vector<Processor> >* sysmem_local_procs,
                 std::map<Processor, Memory>* proc_sysmems,
                 std::map<Processor, Memory>* proc_regmems);
  private:
  MapperRuntime *runtime;
  
  typedef std::map<unsigned, std::vector<Processor>> ProcessorLists;
  ProcessorLists processorLists;
  typedef std::map<unsigned, std::uniform_int_distribution<int>> ProcessorDistribution;
  ProcessorDistribution processorDistribution;
  
  void buildProcessorsLists();
  
  void slice_task(const MapperContext      ctx,
                  const Task&              task,
                  const SliceTaskInput&    input,
                  SliceTaskOutput&   output);
  
  void decompose_points(const Rect<1, coord_t> &point_rect,
                        const Point<1, coord_t> &num_blocks,
                        std::vector<TaskSlice> &slices);
  
  Processor randomProcOfKind(unsigned kind) const;
  
  void select_task_options(const MapperContext    ctx,
                           const Task&            task,
                           TaskOptions&     output);
};


//--------------------------------------------------------------------------
RandomMapper::RandomMapper(MapperRuntime *rt,
                           Machine machine, Processor local,
                           const char *mapper_name,
                           std::vector<Processor>* _procs_list,
                           std::vector<Memory>* _sysmems_list,
                           std::map<Memory, std::vector<Processor> >* _sysmem_local_procs,
                           std::map<Processor, Memory>* _proc_sysmems,
                           std::map<Processor, Memory>* _proc_regmems)
: DefaultMapper(rt, machine, local, mapper_name),
// procs_list(*_procs_list),
sysmems_list(*_sysmems_list),
//sysmem_local_procs(*_sysmem_local_procs),
proc_sysmems(*_proc_sysmems)
// proc_regmems(*_proc_regmems)
//--------------------------------------------------------------------------
{
  rng = std::mt19937(rd());    // random-number engine used (Mersenne-Twister in this case)
  uni = std::uniform_int_distribution<int>(0, (int)steal_target_procs.size() - 1); // guaranteed unbiased
  runtime = rt;
  buildProcessorLists();
}


//--------------------------------------------------------------------------
void RandomMapper::buildProcessorLists()
//--------------------------------------------------------------------------
{
  // sort the processors by kind
  for(std::vector<Processor>::iterator it = procs_list::begin();
      it != procs_list::end(); ++it) {
    Processor processor = *it;
    processorLists[processor.kind()].push_back(processor);
  }
  
  // set up a random number generator for each processor kind
  for(ProcessorLists::iterator it = processorLists::begin();
      it != processorLists::end(); it++) {
    size_t numProcessors = it->second.size();
    processorDistributions[it->first] = std::uniform_int_distribution<int>(0, (int)numProcessors - 1);
  }
}



//--------------------------------------------------------------------------
Legion::Processor RandomMapper::randomProcOfKind(unsigned kind) const
//--------------------------------------------------------------------------
{
  std::vector<Processor> processors = processorLists[kind];
  std::uniform_int_distribution<int> = processorDistributions[kind];
  unsigned index = uni(rng);
  return processors[index];
}


//--------------------------------------------------------------------------
void RandomMapper::select_task_options(const MapperContext    ctx,
                                         const Task&            task,
                                         TaskOptions&     output)
//--------------------------------------------------------------------------
{
  DefaultMapper::VariantInfo variantInfo =
  DefaultMapper::default_find_preferred_variant(task, ctx,
                                                /*needs tight bound*/false,
                                                /*cache result*/true,
                                                Processor::NO_KIND);
  assert(variantInfo.proc_kind != Processor::NO_KIND);
  Processor initial_proc = randomProcOfKind(variantInfo.proc_kind);
  assert(initial_proc.exists());
  
  output.initial_proc = initial_proc;
  output.inline_task = false;
  output.stealable = false;
  output.map_locally = false;
}



//--------------------------------------------------------------------------
void RandomMapper::decompose_points(const Rect<1, coord_t> &point_rect,
                                      const Point<1, coord_t> &num_blocks,
                                      std::vector<TaskSlice> &slices)
//--------------------------------------------------------------------------
{
  long long num_points = point_rect.hi - point_rect.lo + Point<1, coord_t>(1);
  Rect<1, coord_t> blocks(Point<1, coord_t>(0), num_blocks - Point<1, coord_t>(1));
  slices.reserve(blocks.volume());
  
  for (PointInRectIterator<1, coord_t> pir(blocks); pir(); pir++) {
    Point<1, coord_t> block_lo = *pir;
    Point<1, coord_t> block_hi = *pir + Point<1, coord_t>(TASKS_PER_SLICE);
    
    Point<1, coord_t> slice_lo = num_points * block_lo / num_blocks + point_rect.lo;
    Point<1, coord_t> slice_hi = num_points * block_hi / num_blocks +
    point_rect.lo - Point<1, coord_t>(1);
    Rect<1, coord_t> slice_rect(slice_lo, slice_hi);
    
    if (slice_rect.volume() > 0) {
      TaskSlice slice;
      slice.domain = slice_rect;
      slice.proc = local_proc;
      slice.recurse = false;
      slice.stealable = true;
      slices.push_back(slice);
    }
  }
  
}


//--------------------------------------------------------------------------
void RandomMapper::slice_task(const MapperContext      ctx,
                                const Task&              task,
                                const SliceTaskInput&    input,
                                SliceTaskOutput&   output)
//--------------------------------------------------------------------------
{
    Rect<1, coord_t> point_rect = input.domain;
     assert(input.domain.get_dim() == 1);
    
    Point<1, coord_t> num_blocks(point_rect.volume() / TASKS_PER_STEALABLE_SLICE);
    decompose_points(point_rect, num_blocks, output.slices);
}




static void create_mappers(Machine machine, HighLevelRuntime *runtime, const std::set<Processor> &local_procs)
{
  std::vector<Processor>* procs_list = new std::vector<Processor>();
  std::vector<Memory>* sysmems_list = new std::vector<Memory>();
  std::map<Memory, std::vector<Processor> >* sysmem_local_procs =
  new std::map<Memory, std::vector<Processor> >();
  std::map<Processor, Memory>* proc_sysmems = new std::map<Processor, Memory>();
  std::map<Processor, Memory>* proc_regmems = new std::map<Processor, Memory>();
  
  std::vector<Machine::ProcessorMemoryAffinity> proc_mem_affinities;
  machine.get_proc_mem_affinity(proc_mem_affinities);
  
  for (unsigned idx = 0; idx < proc_mem_affinities.size(); ++idx) {
    Machine::ProcessorMemoryAffinity& affinity = proc_mem_affinities[idx];
    if (affinity.p.kind() == Processor::LOC_PROC
        || affinity.p.kind() == Processor::IO_PROC
        || affinity.p.kind() == Processor::PY_PROC) {
      if (affinity.m.kind() == Memory::SYSTEM_MEM) {
        (*proc_sysmems)[affinity.p] = affinity.m;
        if (proc_regmems->find(affinity.p) == proc_regmems->end())
          (*proc_regmems)[affinity.p] = affinity.m;
      }
      else if (affinity.m.kind() == Memory::REGDMA_MEM)
        (*proc_regmems)[affinity.p] = affinity.m;
    }
  }
  
  for (std::map<Processor, Memory>::iterator it = proc_sysmems->begin();
       it != proc_sysmems->end(); ++it) {
    procs_list->push_back(it->first);
    (*sysmem_local_procs)[it->second].push_back(it->first);
  }
  
  for (std::map<Memory, std::vector<Processor> >::iterator it =
       sysmem_local_procs->begin(); it != sysmem_local_procs->end(); ++it)
    sysmems_list->push_back(it->first);
  
  for (std::set<Processor>::const_iterator it = local_procs.begin();
       it != local_procs.end(); it++)
  {
    RandomMapper* mapper = new RandomMapper(runtime->get_mapper_runtime(),
                                                machine, *it, "random_mapper",
                                                procs_list,
                                                sysmems_list,
                                                sysmem_local_procs,
                                                proc_sysmems,
                                                proc_regmems);
    runtime->replace_default_mapper(mapper, *it);
  }
}


void register_random_mapper()
{
  HighLevelRuntime::add_registration_callback(create_mappers);
}
