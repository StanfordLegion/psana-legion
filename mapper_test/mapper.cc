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

#include "mapper.h"


/*
 Goal: write a distributed task pool to serve a set of worker processors.
 
 1. A “takeInput” task runs somewhere, receives job metadata.  Issues an index task launch of “doSomeWork” tasks, with metadata provided per point task.
 
 2. In mapper slice_task the “doSomeWork” tasks are sliced by calling default_slice_task which called default_decompose_points.  This will send the tasks to the ready queues of a set of processors that we classify as “task pool servers”.  These processor hold the tasks so that they can be stolen by worker processors.
 
 3. In mapper select_tasks_to_map prevents “doSomeWork” from being mapped so they will sit in the ready queue.  Mapper permit_steal_request will allow these tasks to be stolen by worker processors.
 
 4. A third set of “worker” processors run “stealWork” tasks that steal work from the task pool servers. In mapper select_steal_targets we specify the target processors belong to the “task pool server” set.
 
 5. In mapper map_task allow doSomeWork to run.
 
 */

#include <map>
#include <random>
#include <vector>

#include "default_mapper.h"

using namespace Legion;
using namespace Legion::Mapping;

//static const char* INPUT_TASK = "takeInput";
static const char* WORKER_TASK = "fetch_and_analyze";
//static const char* STEAL_TASK = "stealWork";

static const int NUM_INPUT_PROCS = 1;
static const int NUM_TASK_POOL_PROCS = 2;
static const int NUM_WORKER_PROCS = 999;

static LegionRuntime::Logger::Category log_psana_mapper("psana_mapper");


///
/// Mapper
///



class PsanaMapper : public DefaultMapper
{
public:
  PsanaMapper(MapperRuntime *rt, Machine machine, Processor local,
              const char *mapper_name,
              std::vector<Processor>* procs_list,
              std::vector<Memory>* sysmems_list,
              std::map<Memory, std::vector<Processor> >* sysmem_local_procs,
              std::map<Processor, Memory>* proc_sysmems,
              std::map<Processor, Memory>* proc_regmems);
private:
  // std::vector<Processor>& procs_list;
  // std::vector<Memory>& sysmems_list;
  //std::map<Memory, std::vector<Processor> >& sysmem_local_procs;
  std::map<Processor, Memory>& proc_sysmems;
  // std::map<Processor, Memory>& proc_regmems;
  std::vector<Processor> input_procs;
  std::vector<Processor> task_pool_procs;
  std::vector<Processor> worker_procs;
  std::random_device rd;     // only used once to initialise (seed) engine
  std::mt19937 rng;
  std::uniform_int_distribution<int> uni;
  
  void categorizeProcessors();
  void slice_task(const MapperContext      ctx,
                  const Task&              task,
                  const SliceTaskInput&    input,
                  SliceTaskOutput&   output);
  void select_tasks_to_map(const MapperContext          ctx,
                           const SelectMappingInput&    input,
                           SelectMappingOutput&   output);
  void select_steal_targets(const MapperContext         ctx,
                            const SelectStealingInput&  input,
                            SelectStealingOutput& output);
  void permit_steal_request(const MapperContext         ctx,
                            const StealRequestInput&    input,
                            StealRequestOutput&   output);
  void map_task(const MapperContext      ctx,
                const Task&              task,
                const MapTaskInput&      input,
                MapTaskOutput&     output);
  void select_task_options(const MapperContext    ctx,
                           const Task&            task,
                           TaskOptions&     output);
  
  const char* get_mapper_name(void) const { return "PsanaMapper"; }
  MapperSyncModel get_mapper_sync_model(void) const {
    return CONCURRENT_MAPPER_MODEL;
  }
};



PsanaMapper::PsanaMapper(MapperRuntime *rt, Machine machine, Processor local,
                         const char *mapper_name,
                         std::vector<Processor>* _procs_list,
                         std::vector<Memory>* _sysmems_list,
                         std::map<Memory, std::vector<Processor> >* _sysmem_local_procs,
                         std::map<Processor, Memory>* _proc_sysmems,
                         std::map<Processor, Memory>* _proc_regmems)
: DefaultMapper(rt, machine, local, mapper_name),
// procs_list(*_procs_list),
// sysmems_list(*_sysmems_list),
//sysmem_local_procs(*_sysmem_local_procs),
proc_sysmems(*_proc_sysmems)
// proc_regmems(*_proc_regmems)
{
  log_psana_mapper.spew("PsanaMapper constructor");
  categorizeProcessors();
  
  rng = std::mt19937(rd());    // random-number engine used (Mersenne-Twister in this case)
  uni = std::uniform_int_distribution<int>(0, task_pool_procs.size()); // guaranteed unbiased
  input_procs = std::vector<Processor>();
  task_pool_procs = std::vector<Processor>();
  worker_procs = std::vector<Processor>();
}


//--------------------------------------------------------------------------
void PsanaMapper::categorizeProcessors()
//--------------------------------------------------------------------------
{
  int num_input = 0;
  int num_task_pool = 0;
  int num_worker = 0;
  
  for(std::map<Processor, Memory>::iterator it = proc_sysmems.begin();
      it != proc_sysmems.end(); it++) {
    if(num_input < NUM_INPUT_PROCS) {
      input_procs.push_back(it->first);
      num_input++;
    } else if(num_task_pool < NUM_TASK_POOL_PROCS) {
      task_pool_procs.push_back(it->first);
      num_task_pool++;
    } else if(num_worker < NUM_WORKER_PROCS) {
      worker_procs.push_back(it->first);
      num_worker++;
    }
  }
  log_psana_mapper.spew("PsanaMapper categorized %d input, %d task pool, %d worker processors",
                        num_input, num_task_pool, num_worker);
}


//--------------------------------------------------------------------------
void PsanaMapper::slice_task(const MapperContext      ctx,
                             const Task&              task,
                             const SliceTaskInput&    input,
                             SliceTaskOutput&   output)
//--------------------------------------------------------------------------
{
  log_psana_mapper.spew("PsanaMapper slice_task task %s", task.get_task_name());
  
  // index task launches pass through here.
  // the only index tasK launch is for WORKER_TASK
  
  assert(!strcmp(task.get_task_name(), WORKER_TASK));
  assert(task.target_proc.kind() == Processor::LOC_PROC);
  
  stealing_enabled = true;
  default_slice_task(task, task_pool_procs, remote_cpus,
                     input, output, cpu_slices_cache);
  
  for(std::vector<TaskSlice>::iterator it = output.slices.begin();
      it != output.slices.end(); it++) {
    it->stealable = true;
  }
}





//--------------------------------------------------------------------------
void PsanaMapper::select_tasks_to_map(const MapperContext          ctx,
                                      const SelectMappingInput&    input,
                                      SelectMappingOutput&   output)
//--------------------------------------------------------------------------
{
  log_psana_mapper.spew("PsanaMapper select_tasks_to_map");
  
  for (std::list<const Task*>::const_iterator it = input.ready_tasks.begin();
       it != input.ready_tasks.end(); it++) {
    const Task* task = *it;
    if(strcmp(task->get_task_name(), WORKER_TASK)) {
      log_psana_mapper.spew("PsanaMapper select_tasks_to_map seleted %s", task->get_task_name());
      output.map_tasks.insert(*it);
    }
  }
}



//--------------------------------------------------------------------------
void PsanaMapper::select_steal_targets(const MapperContext         ctx,
                                       const SelectStealingInput&  input,
                                       SelectStealingOutput& output)
//--------------------------------------------------------------------------
{
  log_psana_mapper.spew("PsanaMapper select_steal_targets");
  
  auto index = uni(rng);
  while(input.blacklist.find(task_pool_procs[index]) != input.blacklist.end()) {
    index = uni(rng);
  }
  output.targets.insert(task_pool_procs[index]);
  log_psana_mapper.spew("PsanaMapper select_steal_targets index %d", index);
}

//--------------------------------------------------------------------------
void PsanaMapper::permit_steal_request(const MapperContext         ctx,
                                       const StealRequestInput&    input,
                                       StealRequestOutput&   output)
//--------------------------------------------------------------------------
{
  log_psana_mapper.spew("PsanaMapper permit_steal_request");
  
  for(unsigned i = 0; i < input.stealable_tasks.size(); ++i) {
    const Task* task = input.stealable_tasks[i];
    if(!strcmp(task->get_task_name(), WORKER_TASK)) {
      output.stolen_tasks.insert(task);
      log_psana_mapper.spew("PsanaMapper permit_steal_request permit stealing %s", task->get_task_name());
    }
  }
}

//--------------------------------------------------------------------------
void PsanaMapper::map_task(const MapperContext      ctx,
                           const Task&              task,
                           const MapTaskInput&      input,
                           MapTaskOutput&     output)
//--------------------------------------------------------------------------
{
  log_psana_mapper.spew("PsanaMapper map_task %s", task.get_task_name());
  if(!strcmp(task.get_task_name(), WORKER_TASK)) {
    log_psana_mapper.spew("PsanaMapper pass task %s to default mapper map_task", task.get_task_name());
    this->DefaultMapper::map_task(ctx, task, input, output);
  }
}




//--------------------------------------------------------------------------
void PsanaMapper::select_task_options(const MapperContext    ctx,
                                      const Task&            task,
                                      TaskOptions&     output)
//--------------------------------------------------------------------------
{
  this->DefaultMapper::select_task_options(ctx, task, output);
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
    if (affinity.p.kind() == Processor::LOC_PROC) {
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
    PsanaMapper* mapper = new PsanaMapper(runtime->get_mapper_runtime(),
                                          machine, *it, "psana_mapper",
                                          procs_list,
                                          sysmems_list,
                                          sysmem_local_procs,
                                          proc_sysmems,
                                          proc_regmems);
    runtime->replace_default_mapper(mapper, *it);
  }
}

void register_mappers()
{
  HighLevelRuntime::add_registration_callback(create_mappers);
}
