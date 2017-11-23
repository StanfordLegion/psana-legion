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
 
 There are n mappers running for n processors.
 Mappers send and respond to these kinds of messages.
 The first two words of the message name indicate the sender and receiver.
 Message arguments are descrined in braces {}.  Here pool proc means the sender
 pool proc and worker proc means the sender worker proc if sender is a worker.
 
 WORKER_POOL_STEAL_REQUEST { worker proc }
 POOL_WORKER_STEAL_ACK { pool proc }
 POOL_WORKER_STEAL_NACK { pool proc }
 POOL_WORKER_WAKEUP ( pool proc }
 POOL_POOL_FORWARD_STEAL { pool proc, worker proc, hops }
 POOL_POOL_FORWARDED_STEAL_SUCCESS { pool proc, worker proc }
 POOL_POOL_WAKEUP_SENT { pool proc, worker proc, hops }
 
 Data structures:
 pendingRequests to be consulted in select_tasks_to_map
 
 Algorithm:
 
 Initially, categorize the mappers into POOL and WORKER type.
 
 First entered in select_steal_targets, send initial
 WORKER_POOL_STEAL_REQUEST { self } from workers to pools.
 
 Worker responds to POOL_WORKER_STEAL_ACK by incrementing its task count.
 
 Worker responds to POOL_WORKER_STEAL_NACK by doing nothing.
 
 Worker responds to POOL_WORKER_WAKEUP by sending WORKER_POOL_STEAL_REQUEST { self }
 to pool proc.
 
 Pool handles WORKER_POOL_STEAL_REQUEST, satisifies it if possible.  (Satisfy means
 set some state that will be consulted in the next invocation of select_tasks_to_map
 [which will use the relocate_tasks option to move tasks to the worker], and send
 POOL_WORKER_STEAL_ACK { self }).  If not satisfied and hops < NUM_TASK_POOL send
 POOL_POOL_FORWARD_STEAL { self, worker proc, hops + 1 } to another random pool.
 
 Pool handles POOL_POOL_FORWARD_STEAL, handle the same as WORKER_POOL_STEAL_REQUEST.
 
 Pool handles POOL_POOL_FORWARDED_STEAL_SUCCESS by removing
 { pool proc, worker proc, hops } from the set of unsatisfied workers and if
 hops > 0 send POOL_POOL_FORWARDED_STEAL_SUCCESS { self, worker proc , hops - 1}
 to pool proc.
 
 Pool handles POOL_POOL_WAKEUP_SENT by removing { pool proc, worker proc, hops }
 from the set of unsatisfied workers.  If hops > 0 send
 POOL_POOL_WAKEUP_SENT { self, worker proc, hops - 1 } to pool proc.
 
 When select_tasks_to_map is invoked with new work the mapper consults the set of
 unsatisfied workers.  For every { pool proc, worker proc, hops } send
 POOL_WORKER_WAKEUP { self } to worker proc, and send
 POOL_POOL_WAKEUP_SENT { self, worker proc, hops } to pool proc.
 
 When task execution completes check task count, if below threshold then send
 WORKER_POOL_STEAL_REQUEST { self } to randomly chosen pool.
 
 */

#include <map>
#include <random>
#include <time.h>
#include <vector>
#include <deque>


#include "default_mapper.h"

using namespace Legion;
using namespace Legion::Mapping;

static const char* WORKER_TASKS[] = { "fetch_and_analyze", "fetch", "analyze" };

static const int NUM_TASK_POOL_MAPPERS = 2;
static const int NUM_WORKER_MAPPERS = 999;
static const int TASKS_PER_STEALABLE_SLICE = 1;

static const int TASK_STEAL_GRANULARITY = 3;
static const int MIN_TASKS_IN_QUEUE = 3;

typedef enum {
  TASK_POOL,
  WORKER
} MapperCategory;

typedef enum {
  WORKER_POOL_STEAL_REQUEST = 1,
  POOL_WORKER_STEAL_ACK,
  POOL_WORKER_STEAL_NACK,
  POOL_WORKER_WAKEUP,
  POOL_POOL_FORWARD_STEAL,
  POOL_POOL_FORWARDED_STEAL_SUCCESS,
  POOL_POOL_WAKEUP_SENT
} MessageType;

typedef struct {
  Processor poolProc;
  Processor workerProc;
  unsigned hops;
} Unsatisfied;


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
  MapperCategory mapperCategory;
  std::map<std::pair<LogicalRegion,Memory>,PhysicalInstance> local_instances;
  typedef long Timestamp;
  unsigned taskQueueSize;
  std::deque<Unsatisfied> pendingRequest;
  std::deque<Unsatisfied> unsatisfied;
  MapperRuntime *runtime;
  MapperEvent defer_select_tasks_to_map;

  Timestamp timeNow() const;
  void categorizeMappers();
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
  bool mapTaskPoolTasks(const MapperContext          ctx,
                        const SelectMappingInput&    input,
                        SelectMappingOutput&   output);
  bool relocateWorkerTasks(const MapperContext          ctx,
                           const SelectMappingInput&    input,
                           SelectMappingOutput&   output,
                           std::deque<Unsatisfied>::const_iterator &pendingRequestIt);
  void forwardUnsatisfiedRequests(const MapperContext          ctx,
                                  const SelectMappingInput&    input,
                                  SelectMappingOutput&   output,
                                  std::deque<Unsatisfied>::const_iterator &pendingRequestIt);
  void select_tasks_to_map(const MapperContext          ctx,
                           const SelectMappingInput&    input,
                           SelectMappingOutput&   output);
  void select_steal_targets(const MapperContext         ctx,
                            const SelectStealingInput&  input,
                            SelectStealingOutput& output);
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
  void maybeSendStealRequest(MapperContext ctx);
  void handle_WORKER_POOL_STEAL_REQUEST(const MapperContext ctx,
                                        const MapperMessage& message);
  void handle_POOL_WORKER_STEAL_ACK(const MapperContext ctx,
                                    const MapperMessage& message);
  void handle_POOL_WORKER_STEAL_NACK(const MapperContext ctx,
                                     const MapperMessage& message);
  void handle_POOL_WORKER_WAKEUP(const MapperContext ctx,
                                 const MapperMessage& message);
  void handle_POOL_POOL_FORWARD_STEAL(const MapperContext ctx,
                                      const MapperMessage& message);
  void handle_POOL_POOL_FORWARDED_STEAL_SUCCESS(const MapperContext ctx,
                                                const MapperMessage& message);
  void handle_POOL_POOL_WAKEUP_SENT(const MapperContext ctx,
                                    const MapperMessage& message);
  virtual void handle_message(const MapperContext           ctx,
                              const MapperMessage&          message);
  
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
  pendingRequest = std::deque<Unsatisfied>();
  unsatisfied = std::deque<Unsatisfied>();
  categorizeMappers();
  
  rng = std::mt19937(rd());    // random-number engine used (Mersenne-Twister in this case)
  uni = std::uniform_int_distribution<int>(0, task_pool_procs.size() - 1); // guaranteed unbiased
  runtime = rt;
  taskQueueSize = 0;
}


//--------------------------------------------------------------------------
void PsanaMapper::maybeSendStealRequest(MapperContext ctx)
//--------------------------------------------------------------------------
{
  assert(mapperCategory == WORKER);
  if(taskQueueSize < MIN_TASKS_IN_QUEUE) {
    int index = uni(rng);
    Processor target = task_pool_procs[index];
    log_psana_mapper.debug("proc %llx: maybeSendStealRequest index %d id %llx",
                           local_proc.id, index, target.id);
    runtime->send_message(ctx, target, &local_proc, sizeof(local_proc),
                          WORKER_POOL_STEAL_REQUEST);
  }
}

//--------------------------------------------------------------------------
void PsanaMapper::handle_WORKER_POOL_STEAL_REQUEST(const MapperContext ctx,
                                                   const MapperMessage& message)
//--------------------------------------------------------------------------
{
  assert(mapperCategory == TASK_POOL);
  // set state to be consulted in select_tasks_to_map
  Processor worker = *(Processor*)message.message;
  Unsatisfied u = { local_proc, worker, 0 };
  pendingRequest.push_back(u);
  if (defer_select_tasks_to_map.exists()){//invoke select_tasks_to_map
    runtime->trigger_mapper_event(ctx, defer_select_tasks_to_map);
    defer_select_tasks_to_map = MapperEvent();
  }
}

//--------------------------------------------------------------------------
void PsanaMapper::handle_POOL_WORKER_STEAL_ACK(const MapperContext ctx,
                                               const MapperMessage& message)
//--------------------------------------------------------------------------
{
  
}

//--------------------------------------------------------------------------
void PsanaMapper::handle_POOL_WORKER_STEAL_NACK(const MapperContext ctx,
                                                const MapperMessage& message)
//--------------------------------------------------------------------------
{
  
}

//--------------------------------------------------------------------------
void PsanaMapper::handle_POOL_WORKER_WAKEUP(const MapperContext ctx,
                                            const MapperMessage& message)
//--------------------------------------------------------------------------
{
  
}

//--------------------------------------------------------------------------
void PsanaMapper::handle_POOL_POOL_FORWARD_STEAL(const MapperContext ctx,
                                                 const MapperMessage& message)
//--------------------------------------------------------------------------
{
  Unsatisfied u = *(Unsatisfied*)message.message;
  pendingRequest.push_back(u);
  if (defer_select_tasks_to_map.exists()){//invoke select_tasks_to_map
    runtime->trigger_mapper_event(ctx, defer_select_tasks_to_map);
    defer_select_tasks_to_map = MapperEvent();
  }
}

//--------------------------------------------------------------------------
void PsanaMapper::handle_POOL_POOL_FORWARDED_STEAL_SUCCESS(const MapperContext ctx,
                                                           const MapperMessage& message)
//--------------------------------------------------------------------------
{
  
}

//--------------------------------------------------------------------------
void PsanaMapper::handle_POOL_POOL_WAKEUP_SENT(const MapperContext ctx,
                                               const MapperMessage& message)
//--------------------------------------------------------------------------
{
}



//--------------------------------------------------------------------------
void PsanaMapper::handle_message(const MapperContext ctx,
                                 const MapperMessage& message)
//--------------------------------------------------------------------------
{
  switch(message.kind) {
    case WORKER_POOL_STEAL_REQUEST:
      handle_WORKER_POOL_STEAL_REQUEST(ctx, message);
      break;
    case POOL_WORKER_STEAL_ACK:
      handle_POOL_WORKER_STEAL_ACK(ctx, message);
      break;
    case POOL_WORKER_STEAL_NACK:
      handle_POOL_WORKER_STEAL_NACK(ctx, message);
      break;
    case POOL_WORKER_WAKEUP:
      handle_POOL_WORKER_WAKEUP(ctx, message);
      break;
    case POOL_POOL_FORWARD_STEAL:
      handle_POOL_POOL_FORWARD_STEAL(ctx, message);
      break;
    case POOL_POOL_FORWARDED_STEAL_SUCCESS:
      handle_POOL_POOL_FORWARDED_STEAL_SUCCESS(ctx, message);
      break;
    case POOL_POOL_WAKEUP_SENT:
      handle_POOL_POOL_WAKEUP_SENT(ctx, message);
      break;
    default: assert(false);
  }
}

//--------------------------------------------------------------------------
PsanaMapper::Timestamp PsanaMapper::timeNow() const
//--------------------------------------------------------------------------
{
  return Realm::Clock::current_time_in_nanoseconds();
}


//--------------------------------------------------------------------------
void PsanaMapper::categorizeMappers()
//--------------------------------------------------------------------------
{
  int num_task_pool = 0;
  int num_worker = 0;
  
  for(std::map<Processor, Memory>::iterator it = proc_sysmems.begin();
      it != proc_sysmems.end(); it++) {
    if(num_task_pool < NUM_TASK_POOL_MAPPERS) {
      log_psana_mapper.debug("proc %llx: categorizeMappers: task pool %llx", local_proc.id, it->first.id);
      task_pool_procs.push_back(it->first);
      if(it->first == local_proc) {
        mapperCategory = TASK_POOL;
      }
      num_task_pool++;
    } else if(num_worker < NUM_WORKER_MAPPERS) {
      log_psana_mapper.debug("proc %llx: categorizeMappers: worker %llx", local_proc.id, it->first.id);
      if(it->first == local_proc) {
        mapperCategory = WORKER;
      }
      worker_procs.push_back(it->first);
      num_worker++;
    }
  }
  log_psana_mapper.debug("proc %llx: categorizeMappers: %d task pool, %d worker processors",
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
    
  } else {
    log_psana_mapper.debug("proc %llx: slice_task pass %s to default mapper",
                           local_proc.id, taskDescription(task));
    this->DefaultMapper::slice_task(ctx, task, input, output);
  }
}


//--------------------------------------------------------------------------
bool PsanaMapper::mapTaskPoolTasks(const MapperContext          ctx,
                                   const SelectMappingInput&    input,
                                   SelectMappingOutput&   output)
//--------------------------------------------------------------------------
{
  bool mapped = false;
  for(std::list<const Task*>::const_iterator taskIt = input.ready_tasks.begin();
      taskIt != input.ready_tasks.end(); taskIt++) {
    const Task* task = *taskIt;
    if(!isWorkerTask(*task)) {
      output.map_tasks.insert(task);
      mapped = true;
      log_psana_mapper.debug("proc %llx: select_tasks_to_map pool maps %s",
                             local_proc.id, taskDescription(*task));
    }
  }
  return mapped;
}

//--------------------------------------------------------------------------
bool PsanaMapper::relocateWorkerTasks(const MapperContext          ctx,
                                      const SelectMappingInput&    input,
                                      SelectMappingOutput&   output,
                                      std::deque<Unsatisfied>::const_iterator &pendingRequestIt)
//--------------------------------------------------------------------------
{
  std::list<const Task*>::const_iterator taskIt = input.ready_tasks.begin();
  pendingRequestIt = pendingRequest.begin();
  bool mapped = false;
  
  // check the pendingRequests, relocate tasks for each entry
  while(pendingRequestIt != pendingRequest.end() && taskIt != input.ready_tasks.end()) {
    Unsatisfied u = *pendingRequestIt;
    unsigned numRelocated = 0;
    log_psana_mapper.debug("proc %llx: select_tasks_to_map pending from %llx",
                           local_proc.id,
                           u.workerProc.id);
    
    while(numRelocated < TASK_STEAL_GRANULARITY && taskIt != input.ready_tasks.end()) {
      const Task* task = *taskIt++;
      if(isWorkerTask(*task)) {
        log_psana_mapper.info("# %lld p %llx remapping %s to %llx",
                              Realm::Clock::current_time_in_nanoseconds(),
                              local_proc.id,
                              taskDescription(*task),
                              u.workerProc.id);
        log_psana_mapper.debug("proc %llx: select_tasks_to_map relocating %s to %llx",
                               local_proc.id,
                               taskDescription(*task),
                               u.workerProc.id);
        output.relocate_tasks[task] = u.workerProc;
        runtime->send_message(ctx, u.workerProc, NULL, 0, POOL_WORKER_STEAL_ACK);
        numRelocated++;
        mapped = true;
      }
    }
    
    if(numRelocated == TASK_STEAL_GRANULARITY) {
      pendingRequest.pop_front();
      pendingRequestIt++;
    }
  }
  return mapped;
}

//--------------------------------------------------------------------------
void PsanaMapper::forwardUnsatisfiedRequests(const MapperContext          ctx,
                                             const SelectMappingInput&    input,
                                             SelectMappingOutput&   output,
                                             std::deque<Unsatisfied>::const_iterator &pendingRequestIt)
//--------------------------------------------------------------------------
{
  while(pendingRequestIt != pendingRequest.end()) {
    Unsatisfied u = *pendingRequestIt++;
    pendingRequest.pop_front();
    unsatisfied.push_back(u);
    if(u.hops < NUM_TASK_POOL_MAPPERS) {
      u.hops++;
      int index = uni(rng);
      Processor target = task_pool_procs[index];
      while(target.id == local_proc.id) {
        index = uni(rng);
        target = task_pool_procs[index];
      }
      runtime->send_message(ctx, target, &u, sizeof(u), POOL_POOL_FORWARD_STEAL);
      log_psana_mapper.debug("proc %llx: select_tasks_to_map send "
                             "POOL_POOL_FORWARD_STEAL for worker %llx to %llx",
                             local_proc.id,
                             u.workerProc.id,
                             target.id);
    }
  }
  
}

//--------------------------------------------------------------------------
void PsanaMapper::select_tasks_to_map(const MapperContext          ctx,
                                      const SelectMappingInput&    input,
                                      SelectMappingOutput&   output)
//--------------------------------------------------------------------------
{
  log_psana_mapper.debug("proc %llx: select_tasks_to_map", local_proc.id);
  
  // TODO in the future task pool can map worker tasks onto itself
  
  if(mapperCategory == TASK_POOL) {
    
    log_psana_mapper.debug("proc %llx: select_tasks_to_map readyTasks size %ld",
                           local_proc.id,
                           input.ready_tasks.size());
    
    bool mapped = mapTaskPoolTasks(ctx, input, output);
    std::deque<Unsatisfied>::const_iterator pendingRequestIt;
    mapped |= relocateWorkerTasks(ctx, input, output, pendingRequestIt);
    forwardUnsatisfiedRequests(ctx, input, output, pendingRequestIt);
    
    if (!mapped && input.ready_tasks.size() > 0) {
      if (!defer_select_tasks_to_map.exists()) {
        defer_select_tasks_to_map = runtime->create_mapper_event(ctx);
      }
      output.deferral_event = defer_select_tasks_to_map;
    }
    
  } else if(mapperCategory == WORKER) {
    
    // We're a worker, always map any tasks that we've stolen
    output.map_tasks.insert(input.ready_tasks.begin(), input.ready_tasks.end());

    // output for tracing
    for (std::list<const Task*>::const_iterator it = input.ready_tasks.begin();
         it != input.ready_tasks.end(); it++) {
      const Task* task = *it;
      log_psana_mapper.debug("proc %llx: select_tasks_to_map worker maps %s",
                             local_proc.id, taskDescription(*task));
    }
  }
}




//--------------------------------------------------------------------------
void PsanaMapper::select_steal_targets(const MapperContext         ctx,
                                       const SelectStealingInput&  input,
                                       SelectStealingOutput& output)
//--------------------------------------------------------------------------
{
  if(mapperCategory == WORKER) {
    maybeSendStealRequest(ctx);
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
  taskQueueSize--;
  maybeSendStealRequest(ctx);
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
  
  if(mapperCategory == WORKER) {
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
      
      taskQueueSize++;
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
  if(mapperCategory == WORKER ||
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

