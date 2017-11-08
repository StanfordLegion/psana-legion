/* Copyright 2017 Stanford University
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

#include "mapper.h"


/*
 Goal: write a distributed task pool to serve a set of worker processors.
 
 1. A “takeInput” task runs somewhere, receives job metadata.  Issues an index task launch of “fetch_and_analyze” tasks, with metadata provided per point task.
 
 2. In mapper slice_task the “fetch_and_analyze” tasks are sliced by calling default_decompose_points.  This will send the tasks to the ready queues of a set of processors that we classify as “task pool servers”.  These processor hold the tasks so that they can be stolen by worker processors.
 
 3. In mapper select_tasks_to_map prevents “fetch_and_analyze” from being mapped so they will sit in the ready queue.  Mapper permit_steal_request will allow these tasks to be stolen by worker processors.
 
 4. A third set of “worker” processors run “stealWork” tasks that steal work from the task pool servers. In mapper select_steal_targets we specify the target processors belong to the “task pool server” set.
 
 5. In mapper map_task allow fetch_and_analyze to run.
 
 */

#include <map>
#include <random>
#include <time.h>
#include <vector>


#include "default_mapper.h"

using namespace Legion;
using namespace Legion::Mapping;

static const char* WORKER_TASKS[] = { "fetch_and_analyze", "fetch", "analyze" };

static const int NUM_TASK_POOL_PROCS = 2;
static const int NUM_WORKER_PROCS = 999;
static const int TASKS_PER_STEALABLE_SLICE = 1;

static const int TASK_STEAL_GRANULARITY = 3;
static const int MIN_TASKS_IN_QUEUE = 3;

typedef enum {
  TASK_POOL,
  WORKER
} ProcessorCategory;


static LegionRuntime::Logger::Category log_psana_mapper("psana_mapper");


///
/// Mapper
///



class PsanaMapper : public DefaultMapper
{
public:
  PsanaMapper(MapperRuntime *rt,
              Machine machine, Processor local,
              const char *mapper_name,
              std::vector<Processor>* procs_list,
              std::vector<Memory>* sysmems_list,
              std::map<Memory, std::vector<Processor> >* sysmem_local_procs,
              std::map<Processor, Memory>* proc_sysmems,
              std::map<Processor, Memory>* proc_regmems);
private:
  // std::vector<Processor>& procs_list;
  std::vector<Memory>& sysmems_list;
  //std::map<Memory, std::vector<Processor> >& sysmem_local_procs;
  std::map<Processor, Memory>& proc_sysmems;
  //  std::map<Processor, Memory>& proc_regmems;
  std::vector<Processor> task_pool_procs;
  std::vector<Processor> worker_procs;
  std::random_device rd;     // only used once to initialise (seed) engine
  std::mt19937 rng;
  std::uniform_int_distribution<int> uni;
  ProcessorCategory processorCategory;
  std::map<std::pair<LogicalRegion,Memory>,PhysicalInstance> local_instances;
  typedef long Timestamp;
  typedef struct {
    unsigned taskQueueSize;
    bool issuedStealRequest;
    Timestamp issuedStealRequestTime;
    Processor issuedStealRequestTarget;
    Timestamp observedRequestLatency;
  } ProcessorStealStatus;
  std::map<long long unsigned int, ProcessorStealStatus> processorStealStatus;
  const Timestamp oneSecond = 1000000000L;
  
  void dumpPSS(long long unsigned id);
  Timestamp timeNow() const;
  bool shouldIssueStealRequest();
  void categorizeProcessors();
  inline char* taskDescription(const Legion::Task& task);
  bool isWorkerTask(const Legion::Task& task);
  void slice_task(const MapperContext      ctx,
                  const Task&              task,
                  const SliceTaskInput&    input,
                  SliceTaskOutput&   output);
  void decompose_points(const Rect<1, coord_t> &point_rect,
                        const std::vector<Processor> &targets,
                        const Point<1, coord_t> &num_blocks,
                        std::vector<TaskSlice> &slices);
  void select_tasks_to_map(const MapperContext          ctx,
                           const SelectMappingInput&    input,
                           SelectMappingOutput&   output);
  void select_steal_targets(const MapperContext         ctx,
                            const SelectStealingInput&  input,
                            SelectStealingOutput& output);
  void permit_steal_request(const MapperContext         ctx,
                            const StealRequestInput&    input,
                            StealRequestOutput&   output);
  Memory get_associated_sysmem(Processor proc);
  void map_task_array(const MapperContext ctx,
                      LogicalRegion region,
                      Memory target,
                      std::vector<PhysicalInstance> &instances);
  void report_profiling(const MapperContext      ctx,
                        const Task&              task,
                        const TaskProfilingInfo& input);
  
  void map_task(const MapperContext      ctx,
                const Task&              task,
                const MapTaskInput&      input,
                MapTaskOutput&     output);
  void select_task_options(const MapperContext    ctx,
                           const Task&            task,
                           TaskOptions&     output);
  void premap_task(const MapperContext      ctx,
                   const Task&              task,
                   const PremapTaskInput&   input,
                   PremapTaskOutput&        output);
  
  const char* get_mapper_name(void) const { return "psana_mapper"; }
  MapperSyncModel get_mapper_sync_model(void) const {
    return SERIALIZED_REENTRANT_MAPPER_MODEL;
  }
};



PsanaMapper::PsanaMapper(MapperRuntime *rt,
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
{
  log_psana_mapper.info("proc %llx: constructor: %lld", local_proc.id,
                        Realm::Clock::current_time_in_nanoseconds());
  
  task_pool_procs = std::vector<Processor>();
  worker_procs = std::vector<Processor>();
  categorizeProcessors();
  
  rng = std::mt19937(rd());    // random-number engine used (Mersenne-Twister in this case)
  uni = std::uniform_int_distribution<int>(0, task_pool_procs.size() - 1); // guaranteed unbiased
}


//--------------------------------------------------------------------------
PsanaMapper::Timestamp PsanaMapper::timeNow() const
//--------------------------------------------------------------------------
{
  return Realm::Clock::current_time_in_nanoseconds();
}


//--------------------------------------------------------------------------
void PsanaMapper::categorizeProcessors()
//--------------------------------------------------------------------------
{
  int num_task_pool = 0;
  int num_worker = 0;
  
  for(std::map<Processor, Memory>::iterator it = proc_sysmems.begin();
      it != proc_sysmems.end(); it++) {
    if(num_task_pool < NUM_TASK_POOL_PROCS) {
      log_psana_mapper.debug("proc %llx: categorizeProcessors: task pool %llx", local_proc.id, it->first.id);
      task_pool_procs.push_back(it->first);
      if(it->first == local_proc) {
        processorCategory = TASK_POOL;
      }
      num_task_pool++;
    } else if(num_worker < NUM_WORKER_PROCS) {
      log_psana_mapper.debug("proc %llx: categorizeProcessors: worker %llx", local_proc.id, it->first.id);
      if(it->first == local_proc) {
        processorCategory = WORKER;
      }
      worker_procs.push_back(it->first);
      ProcessorStealStatus stealStatus = { 0 };
      stealStatus.observedRequestLatency = 10 * oneSecond;
      processorStealStatus[it->first.id] = stealStatus;
      num_worker++;
    }
  }
  log_psana_mapper.debug("proc %llx: categorizeProcessors: %d task pool, %d worker processors",
                         local_proc.id, num_task_pool, num_worker);
}


inline std::string timeNow()
{
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  const unsigned long billion = 1000000000L;
  unsigned long long tt = t.tv_sec * billion + t.tv_nsec;
  char buffer[128];
  sprintf(buffer, "%llu", tt);
  return std::string(buffer);
}


//--------------------------------------------------------------------------
inline char* PsanaMapper::taskDescription(const Legion::Task& task)
//--------------------------------------------------------------------------
{
  static  char buffer[512];
  sprintf(buffer, "%s:%llx%s", task.get_task_name(), task.get_unique_id(),
          (task.is_index_space ?
           (task.index_point.get_dim() > 0 ? ":Point" : ":Index")
           : ""));
  return buffer;
}


//--------------------------------------------------------------------------
bool PsanaMapper::isWorkerTask(const Legion::Task& task)
//--------------------------------------------------------------------------
{
  int numWorkerTasks = sizeof(WORKER_TASKS) / sizeof(WORKER_TASKS[0]);
  for(int i = 0; i < numWorkerTasks; ++i) {
    if(!strcmp(task.get_task_name(), WORKER_TASKS[i])) {
      return true;
    }
  }
  return false;
}


//--------------------------------------------------------------------------
void PsanaMapper::decompose_points(const Rect<1, coord_t> &point_rect,
                                   const std::vector<Processor> &targets,
                                   const Point<1, coord_t> &num_blocks,
                                   std::vector<TaskSlice> &slices)
//--------------------------------------------------------------------------
{
  int num_points = point_rect.hi - point_rect.lo + Point<1, coord_t>(1);
  Rect<1, coord_t> blocks(Point<1, coord_t>(0), num_blocks - Point<1, coord_t>(1));
  size_t next_index = 0;
  slices.reserve(blocks.volume());
  for (PointInRectIterator<1, coord_t> pir(blocks); pir(); pir++) {
    Point<1, coord_t> block_lo = *pir;
    Point<1, coord_t> block_hi = *pir + Point<1, coord_t>(TASKS_PER_STEALABLE_SLICE);
    
    Point<1, coord_t> slice_lo = num_points * block_lo / num_blocks + point_rect.lo;
    Point<1, coord_t> slice_hi = num_points * block_hi / num_blocks +
    point_rect.lo - Point<1, coord_t>(1);
    Rect<1, coord_t> slice_rect(slice_lo, slice_hi);
    
    if (slice_rect.volume() > 0) {
      TaskSlice slice;
      slice.domain = slice_rect;
      slice.proc = targets[next_index++ % targets.size()];
      slice.recurse = false;
      slice.stealable = true;
      slices.push_back(slice);
    }
  }
  
}


//--------------------------------------------------------------------------
void PsanaMapper::slice_task(const MapperContext      ctx,
                             const Task&              task,
                             const SliceTaskInput&    input,
                             SliceTaskOutput&   output)
//--------------------------------------------------------------------------
{
  
  if(isWorkerTask(task)){
    log_psana_mapper.debug("proc %llx: slice_task task %s target proc %llx proc_kind %d",
                           local_proc.id,
                           taskDescription(task),
                           task.target_proc.id,
                           task.target_proc.kind());
    assert(task.target_proc.kind() == Processor::LOC_PROC);
    assert(input.domain.get_dim() == 1);
    
    Rect<1, coord_t> point_rect = input.domain;
    Point<1, coord_t> num_blocks(point_rect.volume() / TASKS_PER_STEALABLE_SLICE);
    decompose_points(point_rect, task_pool_procs, num_blocks, output.slices);
    
    //debug
    for(std::vector<TaskSlice>::iterator it = output.slices.begin();
        it != output.slices.end(); ++it) {
      log_psana_mapper.debug("proc %llx: slice_task %s slice proc %llx stealable %d",
                             local_proc.id,
                             taskDescription(task),
                             it->proc.id,
                             it->stealable);
    }
  } else {
    log_psana_mapper.debug("proc %llx: slice_task pass %s to default mapper",
                           local_proc.id, taskDescription(task));
    this->DefaultMapper::slice_task(ctx, task, input, output);
  }
}


//--------------------------------------------------------------------------
void PsanaMapper::select_tasks_to_map(const MapperContext          ctx,
                                      const SelectMappingInput&    input,
                                      SelectMappingOutput&   output)
//--------------------------------------------------------------------------
{
  log_psana_mapper.debug("proc %llx: select_tasks_to_map", local_proc.id);
  
  if(processorCategory == TASK_POOL) {
    for (std::list<const Task*>::const_iterator it = input.ready_tasks.begin();
         it != input.ready_tasks.end(); it++) {
      const Task* task = *it;
      if(!isWorkerTask(*task)) {
        log_psana_mapper.debug("proc %llx: select_tasks_to_map selected %s",
                               local_proc.id,
                               taskDescription(*task));
        output.map_tasks.insert(*it);
      } else {
        log_psana_mapper.debug("proc %llx: select_tasks_to_map skipping %s",
                               local_proc.id,
                               taskDescription(*task));
      }
    }
  } else {
    for (std::list<const Task*>::const_iterator it = input.ready_tasks.begin();
         it != input.ready_tasks.end(); it++) {
      const Task* task = *it;
      
      log_psana_mapper.debug("proc %llx: select_tasks_to_map worker maps %s",
                             local_proc.id, taskDescription(*task));
    }
    // We're a worker, always map any tasks that we've stolen
    output.map_tasks.insert(input.ready_tasks.begin(), input.ready_tasks.end());
  }
}


//--------------------------------------------------------------------------
void PsanaMapper::dumpPSS(long long unsigned id)
//--------------------------------------------------------------------------
{
  std::map<long long unsigned int, ProcessorStealStatus>::const_iterator it = processorStealStatus.find(local_proc.id);
  assert(it != processorStealStatus.end());
  const ProcessorStealStatus* pss = &it->second;
  log_psana_mapper.debug("proc %llx pss qsize %d issued? %d issueTime %ld observed %ld",
                         id, pss->taskQueueSize, pss->issuedStealRequest,
                         pss->issuedStealRequestTime, pss->observedRequestLatency);
}

//--------------------------------------------------------------------------
bool PsanaMapper::shouldIssueStealRequest()
//--------------------------------------------------------------------------
{
  std::map<long long unsigned int, ProcessorStealStatus>::const_iterator it = processorStealStatus.find(local_proc.id);
  assert(it != processorStealStatus.end());
  dumpPSS(local_proc.id);
  if(it->second.taskQueueSize > MIN_TASKS_IN_QUEUE) return false;
  if(!it->second.issuedStealRequest) return true;
  if(timeNow() - it->second.issuedStealRequestTime > it->second.observedRequestLatency) return true;
  return false;
}

//--------------------------------------------------------------------------
void PsanaMapper::select_steal_targets(const MapperContext         ctx,
                                       const SelectStealingInput&  input,
                                       SelectStealingOutput& output)
//--------------------------------------------------------------------------
{
  
  if(processorCategory == WORKER) {
    std::map<long long unsigned int, ProcessorStealStatus>::iterator it = processorStealStatus.find(local_proc.id);
    assert(it != processorStealStatus.end());

    if(shouldIssueStealRequest()) {
      log_psana_mapper.debug("proc %llx: select_steal_targets should issue steal request, yes", local_proc.id);
      
      // select a steal target
      
      bool found = false;
      unsigned counter = 0;
      int index;
      
      while(!found) {
        index = uni(rng);
        log_psana_mapper.debug("proc %llx: select_steal_targets trying %d",
                               local_proc.id, index);
        if(input.blacklist.find(task_pool_procs[index]) != input.blacklist.end()) {
          log_psana_mapper.debug("proc %llx: select_steal_targets proc %d (%llx) is in blacklist",
                                 local_proc.id, index, task_pool_procs[index].id);
          if(counter++ >= task_pool_procs.size() * 2) {
            log_psana_mapper.debug("proc %llx: select_steal_targets cannot steal, all procs in blacklist",
                                   local_proc.id);
            return;
          }
        } else {
          found = true;
          log_psana_mapper.debug("proc %llx: select_steal_Targets found %d",
                                 local_proc.id, index);
        }
      }
      
      // issue a steal request
      
      it->second.issuedStealRequestTime = timeNow();
      it->second.issuedStealRequest = true;
      it->second.issuedStealRequestTarget = task_pool_procs[index];
      output.targets.insert(it->second.issuedStealRequestTarget);
      log_psana_mapper.debug("proc %llx: select_steal_targets index %d id %llx",
                             local_proc.id, index,
                             it->second.issuedStealRequestTarget.id);
      log_psana_mapper.info("# %lld p %llx thiefrequest %llx",
                            Realm::Clock::current_time_in_nanoseconds(),
                            local_proc.id,
                            it->second.issuedStealRequestTarget.id);
      
    } else if(it->second.issuedStealRequest) {
      
      log_psana_mapper.debug("proc %llx: select_steal_targets don't steal because outstanding request to %llx",
                             local_proc.id, it->second.issuedStealRequestTarget.id);
      // check to see if recent request caused proc to go on blacklist
      //if(input.blacklist.find(it->second.issuedStealRequestTarget) != input.blacklist.end()) {
        Timestamp elapsedTime = timeNow() - it->second.issuedStealRequestTime;
      bool targetOnBlacklist = input.blacklist.find(it->second.issuedStealRequestTarget) != input.blacklist.end();
        log_psana_mapper.debug("proc %llx: select_steal_targets issued, target %llx on blacklist? %d, elapsed %ld",
                               local_proc.id,
                               it->second.issuedStealRequestTarget.id,
                               targetOnBlacklist,
                               elapsedTime);
        if(elapsedTime >= 3 * it->second.observedRequestLatency) {
          // assume this proc went to the blacklist because of our request.
          // this might not be true (another worker request might have caused the blacklist).
          // in that case we will falsely conclude that the request failed.
          it->second.issuedStealRequest = false;
          log_psana_mapper.debug("proc %llx: select_steal_targets outmake standing request to proc %llx has expired",
                                 local_proc.id, it->second.issuedStealRequestTarget.id);
        }
        
      //}
      
    } else {
      log_psana_mapper.debug("proc %llx: select_steal_targets should not issue request",
                             local_proc.id);
    }
    
  } else {
    log_psana_mapper.debug("proc %llx: select_steal_targets skipped because not worker",
                           local_proc.id);
  }
}

//--------------------------------------------------------------------------
void PsanaMapper::permit_steal_request(const MapperContext         ctx,
                                       const StealRequestInput&    input,
                                       StealRequestOutput&   output)
//--------------------------------------------------------------------------
{
  
  if(processorCategory == TASK_POOL) {
    log_psana_mapper.debug("proc %llx: permit_steal_request, stealable_tasks.size %lu",
                           local_proc.id, input.stealable_tasks.size());
    log_psana_mapper.info("# %lld p %llx stealbythief %llx",
                          Realm::Clock::current_time_in_nanoseconds(),
                          local_proc.id,
                          input.thief_proc.id);
    for(unsigned i = 0;
        output.stolen_tasks.size() < TASK_STEAL_GRANULARITY
        && i < input.stealable_tasks.size();
        ++i) {
      const Task* task = input.stealable_tasks[i];
      if(isWorkerTask(*task)) {
        output.stolen_tasks.insert(task);
        log_psana_mapper.debug("proc %llx: permit_steal_request permits stealing %s",
                               local_proc.id, taskDescription(*task));
      } else {
        log_psana_mapper.debug("proc %llx: permit_steal_request does not permit stealing %s",
                               local_proc.id, taskDescription(*task));
      }
    }
  } else {
    log_psana_mapper.debug("proc %llx: permit_steal_request skipped because not task pool",
                           local_proc.id);
  }
}

//------------------------------------------------------------------------------
Memory PsanaMapper::get_associated_sysmem(Processor proc)
//------------------------------------------------------------------------------
{
  std::map<Processor,Memory>::const_iterator finder =
  proc_sysmems.find(proc);
  if (finder != proc_sysmems.end())
    return finder->second;
  Machine::MemoryQuery sysmem_query(machine);
  sysmem_query.same_address_space_as(proc);
  sysmem_query.only_kind(Memory::SYSTEM_MEM);
  Memory result = sysmem_query.first();
  assert(result.exists());
  proc_sysmems[proc] = result;
  return result;
}

//--------------------------------------------------------------------------
void PsanaMapper::map_task_array(const MapperContext ctx,
                                 LogicalRegion region,
                                 Memory target,
                                 std::vector<PhysicalInstance> &instances)
//--------------------------------------------------------------------------
{
  const std::pair<LogicalRegion,Memory> key(region, target);
  std::map<std::pair<LogicalRegion,Memory>,PhysicalInstance>::const_iterator
  finder = local_instances.find(key);
  if (finder != local_instances.end()) {
    instances.push_back(finder->second);
    return;
  }
  
  std::vector<LogicalRegion> regions(1, region);
  LayoutConstraintSet layout_constraints;
  
  // Constrained for the target memory kind
  layout_constraints.add_constraint(MemoryConstraint(target.kind()));
  
  // Have all the field for the instance available
  std::vector<FieldID> all_fields;
  runtime->get_field_space_fields(ctx, region.get_field_space(), all_fields);
  layout_constraints.add_constraint(FieldConstraint(all_fields, false/*contiguous*/,
                                                    false/*inorder*/));
  
  PhysicalInstance result;
  bool created;
  if (!runtime->find_or_create_physical_instance(ctx, target, layout_constraints,
                                                 regions, result, created, true/*acquire*/, GC_NEVER_PRIORITY)) {
    log_psana_mapper.error("PSANA mapper failed to allocate instance");
    assert(false);
  }
  instances.push_back(result);
  local_instances[key] = result;
}

//--------------------------------------------------------------------------
void PsanaMapper::report_profiling(const MapperContext      ctx,
                                   const Task&              task,
                                   const TaskProfilingInfo& input)
//--------------------------------------------------------------------------
{
  std::map<long long unsigned int, ProcessorStealStatus>::iterator it = processorStealStatus.find(local_proc.id);
  assert(it != processorStealStatus.end());
  it->second.taskQueueSize--;
}

//--------------------------------------------------------------------------
void PsanaMapper::map_task(const MapperContext      ctx,
                           const Task&              task,
                           const MapTaskInput&      input,
                           MapTaskOutput&     output)
//--------------------------------------------------------------------------
{
  Processor::Kind target_kind = task.target_proc.kind();
  VariantInfo chosen = default_find_preferred_variant(task, ctx,
                                                      true/*needs tight bound*/, false/*cache*/, target_kind);
  
  if(processorCategory == WORKER) {
    if(isWorkerTask(task)) {
      
      // this tasks succeeds in running on a worker
      
      log_psana_mapper.debug("proc %llx: map_task worker maps %s to itself, successfully stolen",
                             local_proc.id, taskDescription(task));
      output.chosen_variant = chosen.variant;
      output.task_priority = 0;
      output.postmap_task = false;
      output.target_procs.push_back(local_proc);
      ProfilingRequest completionRequest;
      completionRequest.add_measurement<Realm::ProfilingMeasurements::OperationStatus>();
      output.task_prof_requests = completionRequest;
      
      std::map<long long unsigned int, ProcessorStealStatus>::iterator it = processorStealStatus.find(local_proc.id);
      assert(it != processorStealStatus.end());

      it->second.taskQueueSize++;
      if(it->second.issuedStealRequest) {
        it->second.observedRequestLatency = timeNow() - it->second.issuedStealRequestTime;
        it->second.issuedStealRequest = false;
      }
    } else {
      log_psana_mapper.debug("proc %llx: map_task pass %s to default mapper map_task",
                             local_proc.id, taskDescription(task));
      this->DefaultMapper::map_task(ctx, task, input, output);
    }
  } else {
    log_psana_mapper.debug("proc %llx: map_task non-worker maps %s to itself",
                           local_proc.id, taskDescription(task));
    output.chosen_variant = chosen.variant;
    output.task_priority = 0;
    output.postmap_task = false;
    output.target_procs.push_back(local_proc);
  }
  
  for (unsigned idx = 0; idx < task.regions.size(); idx++) {
    if (task.regions[idx].privilege == NO_ACCESS)
      continue;
    Memory target_mem = get_associated_sysmem(task.target_proc);
    map_task_array(ctx, task.regions[idx].region, target_mem,
                   output.chosen_instances[idx]);
  }
  runtime->acquire_instances(ctx, output.chosen_instances);
  
}


//--------------------------------------------------------------------------
void PsanaMapper::select_task_options(const MapperContext    ctx,
                                      const Task&            task,
                                      TaskOptions&     output)
//--------------------------------------------------------------------------
{
  if(processorCategory == WORKER ||
     !isWorkerTask(task) ||
     task.is_index_space) {
    log_psana_mapper.debug("proc %llx: select_task_options %s on self proc",
                           local_proc.id, taskDescription(task));
    output.initial_proc = local_proc;
    output.inline_task = false;
    output.stealable = false;
    output.map_locally = false;
  } else {
    log_psana_mapper.debug("proc %llx: select_task_options skipping %s",
                           local_proc.id, taskDescription(task));
  }
}


/**
 * ----------------------------------------------------------------------
 *  Premap Task
 * ----------------------------------------------------------------------
 * This mapper call is only invoked for tasks which either explicitly
 * requested it by setting 'premap_task' in the 'select_task_options'
 * mapper call or by having a region requirement which needs to be
 * premapped (e.g. an in index space task launch with an individual
 * region requirement with READ_WRITE EXCLUSIVE privileges that all
 * tasks must share). The mapper is told the indicies of which
 * region requirements need to be premapped in the 'must_premap' set.
 * All other regions can be optionally mapped. The mapper is given
 * a vector containing sets of valid PhysicalInstances (if any) for
 * each region requirement.
 *
 * The mapper performs the premapping by filling in premapping at
 * least all the required premapped regions and indicates all premapped
 * region indicies in 'premapped_region'. For each region requirement
 * the mapper can specify a ranking of PhysicalInstances to re-use
 * in 'chosen_ranking'. This can optionally be left empty. The mapper
 * can also specify constraints on the creation of a physical instance
 * in 'layout_constraints'. Finally, the mapper can force the creation
 * of a new instance if an write-after-read dependences are detected
 * on existing physical instances by enabling the WAR optimization.
 * All vector data structures are size appropriately for the number of
 * region requirements in the task.
 */
//------------------------------------------------------------------------
void PsanaMapper::premap_task(const MapperContext      ctx,
                              const Task&              task,
                              const PremapTaskInput&   input,
                              PremapTaskOutput&        output)
//------------------------------------------------------------------------
{
  log_psana_mapper.debug("proc %llx: premap_task %s",
                         local_proc.id, taskDescription(task));
  this->DefaultMapper::premap_task(ctx, task, input, output);
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

