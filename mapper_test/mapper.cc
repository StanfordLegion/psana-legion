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
 
 WORKER_POOL_STEAL_REQUEST
 POOL_WORKER_WAKEUP
 POOL_POOL_FORWARD_STEAL
 POOL_POOL_FORWARD_STEAL_SUCCESS
 POOL_POOL_WAKEUP_SENT{_UP,_DOWN}
 
 Message payloads, also used as request record:
 { source, destination, worker, numTasks, hops, unique-d, triedToSatisfy }
 
 Algorithm:
 
 Initially, categorize the mappers into POOL and WORKER type.
 Each worker sends initially WORKER_POOL_STEAL_REQUEST to pools.
 Each worker subsequently sends WORKER_POOL_STEAL_REQUEST to pools whenever it
 completes a task.
 
 Pool handles WORKER_POOL_STEAL_REQUEST.  If work is available relocate one task.
 If work is not available store the request and mark as "triedToSatisfy", and forward
 the request by sending POOL_POOL_FORWARD_STEAL to a different pool mapper.  This
 message includes a unique id, the identities of the sender and next recipient
 and a hop count of zero.
 
 Pool handles POOL_POOL_FORWARD_STEAL.  If work is available relocate one task
 and send POOL_POOL_FORWARD_STEAL_SUCCESS to the source processor.
 If work is not available store the request, mark it as "triedToSatisfy" and check
 the number of hops; if hops < num pools then forward POOL_POOL_FORWARD_STEAL to
 another pool with an incremented hop count and mark request.destination as
 that pool proc.
 
 Pool handles POOL_POOL_FORWARD_STEAL_SUCCESS.  Delete the stored request by
 unique id and send POOL_POOL_FORWARD_STEAL_SUCCESS to request.source.
 
 Pool sometimes spontaneously receives new work.  For each stored request that
 is marked "triedToSatisfy" send POOL_WORKER_WAKEUP to request.worker.
 Send POOL_POOL_WAKEUP_SENT_UP to request.destination and send
 POOL_POOL_WAKEUP_SENT_DOWN to request.source.  In either forward or reverse
 case don't send a message if we are at the end of a chain (ie hops == 0 or
 hops == num pools).
 
 Worker handles POOL_WORKER_WAKEUP.  If workload is below threshold then send
 WORKER_POOL_STEAL_REQUEST to pools.
 
 Pool handles POOL_POOL_WAKEUP_SENT{_FORWARD,_REVERSE}.  Delete the stored requst
 by unique id.  If (hops != 0 && hops != num pools) then send same message
 along to either request.source or request.destination.
 
 
 Verify these cases informally:
 a -- single thread, no preemption, no other transactions
 b -- single thread, preempted by another transaction
 c -- single thread, no preemption, other simultaneous transactions
 (does b imply c?)
 
 1. Worker w send WORKER_POOL_STEAL_REQUEST to pool.  Pool satisfies request
 immediately.
 2. Worker w sends WORKER_POOL_STEAL_REQUEST to pool p0.  p0 cannot satisfy request
 and sends POOL_POOL_FORWARD_STEAL to a series of pools p1 ... pn.  Pool pn
 satisfies the request and send POOL_POOL_FORWARD_STEAL_SUCCESS to pools p0...pn-1
 3. Worker w sends WORKER_POOL_STEAL_REQUEST to pool p0.  p0 cannot satisfy the
 request and neither can any other pools so request is marked "triedToSatisfy" in
 every pool.  Pool pi spontaneously gets work, sends POOL_WORKER_WAKEUP to
 original worker w, and sends POOL_POOL_WAKEUP_SENT to all other pools.
 Worker w sends WORKER_POOL_STEAL_REQUEST to pool.
 4. Same as 2 but while POOL_POOL_FORWARD_STEAL is being forwarded another
 WORKER_POOL_STEAL_REQUEST comes in and causes another set of messages to happen
 in parallel with the first set.
 5. Same as 3 but multiple pools spontaneously get work at the same time.
 6. Same as 3 but a new POOL_POOL_FORWARD_STEAL comes in immediately after a
 pool issues the first POOL_POOL_WAKEUP_SENT.
 7. Same as 3 but a new POOL_POOL_FORWARD_STEAL sequence is in progress before any
 pool issues the first POOL_POOL_WAKEUP_SENT.
 8. Multiple unsatisfied worker requests lead to multiple simultaneous chains of
 POOL_POOL_FORWARD_STEAL.
 
 
 Implementation in mapper API:
 data structure deque<request record> in each mapper
 no threadsafety issues
 
 First entered in select_steal_targets, each worker sends WORKER_POOL_STEAL_REQUEST
 to pool.
 
 Pool handles WORKER_POOL_STEAL_REQUEST.  Add request to deque to be handled in
 select_tasks_to_map.  Trigger select_tasks_to_map by MapperEvent.
 
 Pool handles POOL_POOL_FORWARD_STEAL.  Add request to deque to be handled in
 select_tasks_to_map.  Trigger select_tasks_to_map by MapperEvent.
 
 select_tasks_to_map() for pool mappers:
 * for every available task that is pool-type, map it to the the local proc.
 * for every deque request that is not marked "triedToSatisfy":
 try to relocate a number of tasks to the requesting worker.  If successful
 send POOL_POOL_FORWARD_STEAL_SUCCESS to the immediate sender if hops > 0 and
 discard the request.  If unsuccessful mark the request "triedToSatisfy", leave
 it in the deque, and send POOL_POOL_FORWARD_STEAL to another pool if
 hops < num pools.
 * if work has spontaneously materialized or there are unmapped tasks in the ready queue:
 for every "triedToSatisfy" request in deque, send POOL_WORKER_WAKEUP to original
 requestor, and send POOL_POOL_WAKEUP_SENT to both immediate sender and also
 next pool.
 
 select_tasks_to_map() for worker mappers:
 map all worker tasks to local proc.
 
 Pool handles POOL_POOL_FORWARD_STEAL_SUCCESS.  Delete request from deque.  If
 hops > 0 send POOL_POOL_FORWARD_STEAL_SUCCESS to immediate sender.
 
 Worker handles POOL_WORKER_WAKEUP.  Worker sends WORKER_POOL_STEAL_REQUEST if
 workload is below threshold.
 
 Pool handles POOL_POOL_WAKEUP_SENT{_UP,_DOWN}.  Exactly like the algorithm.
 
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

static const int MIN_TASKS_IN_QUEUE = 3;

typedef enum {
  TASK_POOL,
  WORKER
} MapperCategory;

typedef enum {
  WORKER_POOL_STEAL_REQUEST = 1,
  POOL_WORKER_WAKEUP,
  POOL_POOL_FORWARD_STEAL,
  POOL_POOL_FORWARD_STEAL_SUCCESS,
  POOL_POOL_WAKEUP_SENT_UP,
  POOL_POOL_WAKEUP_SENT_DOWN
} MessageType;

typedef struct {
  int id;
  Processor sourceProc;
  Processor destinationProc;
  Processor workerProc;
  unsigned numTasks;
  unsigned hops;
  bool triedToSatisfy;
} Request;


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
  std::deque<Request> pendingRequest;
  MapperRuntime *runtime;
  MapperEvent defer_select_tasks_to_map;
  unsigned numUniqueIds;
  
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
                           unsigned& numUnmappedWorkerTasks);
  void wakeUpWorkers(const MapperContext          ctx,
                     const SelectMappingInput&    input,
                     SelectMappingOutput&   output,
                     unsigned numAvailableTasks);
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
  void maybeSendStealRequest(MapperContext ctx, Processor target);
  void triggerSelectTasksToMap(const MapperContext ctx, Request r);
  void handle_WORKER_POOL_STEAL_REQUEST(const MapperContext ctx,
                                        const MapperMessage& message);
  void handle_POOL_WORKER_WAKEUP(const MapperContext ctx,
                                 const MapperMessage& message);
  void handle_POOL_POOL_FORWARD_STEAL(const MapperContext ctx,
                                      const MapperMessage& message);
  void handle_POOL_POOL_FORWARD_STEAL_SUCCESS(const MapperContext ctx,
                                                const MapperMessage& message);
  void handle_POOL_POOL_WAKEUP_SENT(const MapperContext ctx,
                                    const MapperMessage& message,
                                    bool isForward);
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
  pendingRequest = std::deque<Request>();
  categorizeMappers();
  
  rng = std::mt19937(rd());    // random-number engine used (Mersenne-Twister in this case)
  uni = std::uniform_int_distribution<int>(0, task_pool_procs.size() - 1); // guaranteed unbiased
  runtime = rt;
  taskQueueSize = 0;
  numUniqueIds = 0;
}


//--------------------------------------------------------------------------
void PsanaMapper::maybeSendStealRequest(MapperContext ctx, Processor target)
//--------------------------------------------------------------------------
{
  assert(mapperCategory == WORKER);
  if(taskQueueSize < MIN_TASKS_IN_QUEUE) {
    log_psana_mapper.debug("proc %llx: maybeSendStealRequest id %llx",
                           local_proc.id, target.id);
    unsigned numTasks = MIN_TASKS_IN_QUEUE - taskQueueSize;
    Request r = { -1, local_proc, target, local_proc, numTasks, 0, false };
    runtime->send_message(ctx, target, &r, sizeof(r), WORKER_POOL_STEAL_REQUEST);
    log_psana_mapper.debug("proc %llx: send WORKER_POOL_STEAL_REQUEST to %llx",
                           local_proc.id, target.id);
  }
}

//--------------------------------------------------------------------------
void PsanaMapper::maybeSendStealRequest(MapperContext ctx)
//--------------------------------------------------------------------------
{
  int index = uni(rng);
  Processor target = task_pool_procs[index];
  maybeSendStealRequest(ctx, target);
}

//--------------------------------------------------------------------------
unsigned unsatisfiedUniqueId(unsigned& numUniqueIds, unsigned long long processorId)
//--------------------------------------------------------------------------
{
  unsigned processorSerialId = processorId % NUM_TASK_POOL_MAPPERS;
  unsigned result = numUniqueIds++ * NUM_TASK_POOL_MAPPERS + processorSerialId;
  return result;
}

//--------------------------------------------------------------------------
void PsanaMapper::triggerSelectTasksToMap(const MapperContext ctx, Request r)
//--------------------------------------------------------------------------
{
  pendingRequest.push_back(r);
  if (defer_select_tasks_to_map.exists()){
    runtime->trigger_mapper_event(ctx, defer_select_tasks_to_map);
    defer_select_tasks_to_map = MapperEvent();
  }
}

//--------------------------------------------------------------------------
void PsanaMapper::handle_WORKER_POOL_STEAL_REQUEST(const MapperContext ctx,
                                                   const MapperMessage& message)
//--------------------------------------------------------------------------
{
  assert(mapperCategory == TASK_POOL);
  Request r = *(Request*)message.message;
  r.id = unsatisfiedUniqueId(numUniqueIds, local_proc.id);
  log_psana_mapper.debug("proc %llx: handle_WORKER_POOL_STEAL_REQUEST id %d hops %d from worker %llx",
                         local_proc.id, r.id, r.hops, r.workerProc.id);
 triggerSelectTasksToMap(ctx, r);
}


//--------------------------------------------------------------------------
void PsanaMapper::handle_POOL_WORKER_WAKEUP(const MapperContext ctx,
                                            const MapperMessage& message)
//--------------------------------------------------------------------------
{
  assert(mapperCategory == WORKER);
  Request r = *(Request*)message.message;
  log_psana_mapper.debug("proc %llx: handle_POOL_WORKER_WAKEUP id %d numTasks %d from %llx",
                         local_proc.id, r.id, r.numTasks, r.sourceProc.id);
  maybeSendStealRequest(ctx, r.sourceProc);
}

//--------------------------------------------------------------------------
void PsanaMapper::handle_POOL_POOL_FORWARD_STEAL(const MapperContext ctx,
                                                 const MapperMessage& message)
//--------------------------------------------------------------------------
{
  assert(mapperCategory == TASK_POOL);
  Request r = *(Request*)message.message;
  log_psana_mapper.debug("proc %llx: handle_POOL_POOL_FORWARD_STEAL id %d hops %d from worker %llx",
                         local_proc.id, r.id, r.hops, r.workerProc.id);
  triggerSelectTasksToMap(ctx, r);
}

//--------------------------------------------------------------------------
void PsanaMapper::handle_POOL_POOL_FORWARD_STEAL_SUCCESS(const MapperContext ctx,
                                                           const MapperMessage& message)
//--------------------------------------------------------------------------
{
  assert(mapperCategory == TASK_POOL);
  Request r = *(Request*)message.message;
  log_psana_mapper.debug("proc %llx: handle_POOL_POOL_FORWARD_STEAL_SUCCESS id %d hops %d from %llx",
                         local_proc.id, r.id, r.hops, r.sourceProc.id);

  for(std::deque<Request>::iterator it = pendingRequest.begin();
      it != pendingRequest.end(); ) {
    Request v = *it;
    if(r.id == v.id && r.hops == v.hops + 1) {
      it = pendingRequest.erase(it);
      if(v.hops > 0) {
        runtime->send_message(ctx, v.sourceProc, &v, sizeof(v), POOL_POOL_FORWARD_STEAL_SUCCESS);
        log_psana_mapper.debug("proc %llx: send POOL_POOL_FORWARD_STEAL_SUCCESS id %d hops %d to %llx",
                               local_proc.id, v.id, v.hops, v.sourceProc.id);
      }
    } else {
      it++;
    }
  }
}


//--------------------------------------------------------------------------
void PsanaMapper::handle_POOL_POOL_WAKEUP_SENT(const MapperContext ctx,
                                               const MapperMessage& message,
                                               bool isForward)
//--------------------------------------------------------------------------
{
  assert(mapperCategory == TASK_POOL);
  Request r = *(Request*)message.message;
  log_psana_mapper.debug("proc %llx: handle_POOL_POOL_WAKEUP_SENT id %d hops %d from %llx",
                         local_proc.id, r.id, r.hops, r.sourceProc.id);

  for(std::deque<Request>::iterator it = pendingRequest.begin();
      it != pendingRequest.end(); ) {
    Request v = *it;
    unsigned expectedHops = isForward ? v.hops - 1 : v.hops + 1;
    if(r.id == v.id && r.hops == expectedHops) {
      it = pendingRequest.erase(it);
      bool sendFurther = isForward ? v.hops < NUM_TASK_POOL_MAPPERS : v.hops > 0;
      if(sendFurther) {
        unsigned messageType = isForward ? POOL_POOL_WAKEUP_SENT_UP
        : POOL_POOL_WAKEUP_SENT_DOWN;
        runtime->send_message(ctx, v.sourceProc, &v, sizeof(v), messageType);
        log_psana_mapper.debug("proc %llx: send POOL_POOL_WAKEUP_SENT_{UP/DOWN} hops %d to %llx",
                               local_proc.id, v.hops, v.sourceProc.id);
      }
    } else {
      it++;
    }
  }
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
    case POOL_WORKER_WAKEUP:
      handle_POOL_WORKER_WAKEUP(ctx, message);
      break;
    case POOL_POOL_FORWARD_STEAL:
      handle_POOL_POOL_FORWARD_STEAL(ctx, message);
      break;
    case POOL_POOL_FORWARD_STEAL_SUCCESS:
      handle_POOL_POOL_FORWARD_STEAL_SUCCESS(ctx, message);
      break;
    case POOL_POOL_WAKEUP_SENT_UP:
      handle_POOL_POOL_WAKEUP_SENT(ctx, message, true);
      break;
    case POOL_POOL_WAKEUP_SENT_DOWN:
      handle_POOL_POOL_WAKEUP_SENT(ctx, message, false);
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
      taskIt != input.ready_tasks.end(); ) {
    const Task* task = *taskIt++;
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
                                      unsigned& numUnmappedWorkerTasks)
//--------------------------------------------------------------------------
{
  bool mapped = false;
  numUnmappedWorkerTasks = 0;
  std::deque<Request>::iterator pendingRequestIt = pendingRequest.begin();
  std::list<const Task*>::const_iterator taskIt = input.ready_tasks.begin();
  
  while(pendingRequestIt != pendingRequest.end() && taskIt != input.ready_tasks.end()) {
    Request r = *pendingRequestIt;
    log_psana_mapper.debug("proc %llx: select_tasks_to_map pending from %llx",
                           local_proc.id,
                           r.workerProc.id);
    
    // only process new requests
    if(!r.triedToSatisfy) {
      r.triedToSatisfy = true;
      *pendingRequestIt = r;
      
      // try to relocate the requested number of worker tasks
      while(r.numTasks > 0 && taskIt != input.ready_tasks.end()) {
        const Task* task = *taskIt++;
        
        if(isWorkerTask(*task)) {
          log_psana_mapper.info("# %lld p %llx remapping %s to %llx",
                                Realm::Clock::current_time_in_nanoseconds(),
                                local_proc.id,
                                taskDescription(*task),
                                r.workerProc.id);
          log_psana_mapper.debug("proc %llx: select_tasks_to_map relocating %s to %llx",
                                 local_proc.id,
                                 taskDescription(*task),
                                 r.workerProc.id);
          output.relocate_tasks[task] = r.workerProc;
          r.numTasks--;
          mapped = true;
        }
      }
      
      // if the request was not fully satisfied and we are out of tasks
      //  then forward a steal request to another task pool mapper
      if(r.numTasks > 0 && taskIt == input.ready_tasks.end()) {
        if(r.hops < NUM_TASK_POOL_MAPPERS) {
          Request v = r;
          v.hops++;
          v.sourceProc = local_proc;
          int index = uni(rng);
          Processor target = task_pool_procs[index];
          while(target.id == local_proc.id) {
            index = uni(rng);
            target = task_pool_procs[index];
          }
          v.destinationProc = target;
          runtime->send_message(ctx, target, &v, sizeof(v), POOL_POOL_FORWARD_STEAL);
          log_psana_mapper.debug("proc %llx: send POOL_POOL_FORWARD_STEAL to %llx",
                                 local_proc.id, target.id);
        }
      }
      
      if(r.numTasks == 0) {
        pendingRequestIt = pendingRequest.erase(pendingRequestIt);
      } else {
        pendingRequestIt++;
      }
    } else {//!r.triedToSatisfy
      pendingRequestIt++;
    }
  }
  
  // count the number of unmapped worker tasks
  while(taskIt != input.ready_tasks.end()) {
    if(isWorkerTask(**taskIt)) {
      numUnmappedWorkerTasks++;
    }
    taskIt++;
  }
  return mapped;
}




//--------------------------------------------------------------------------
void PsanaMapper::wakeUpWorkers(const MapperContext          ctx,
                                const SelectMappingInput&    input,
                                SelectMappingOutput&   output,
                                unsigned numAvailableTasks)
//--------------------------------------------------------------------------
{
  std::deque<Request>::iterator pendingRequestIt = pendingRequest.begin();
  std::list<const Task*>::const_iterator taskIt = input.ready_tasks.begin();
  
  while(pendingRequestIt != pendingRequest.end() && numAvailableTasks > 0) {
    Request r = *pendingRequestIt;
    if(r.triedToSatisfy) {
      pendingRequestIt = pendingRequest.erase(pendingRequestIt);
      Request v = r;
      v.sourceProc = local_proc;
      v.destinationProc = r.workerProc;
      runtime->send_message(ctx, v.destinationProc, &v, sizeof(v), POOL_WORKER_WAKEUP);
      log_psana_mapper.debug("proc %llx: send POOL_WORKER_WAKEUP to worker %llx",
                             local_proc.id, v.destinationProc.id);
      
      if(r.hops > 0) {
        v = r;
        v.sourceProc = local_proc;
        v.destinationProc = r.sourceProc;
        runtime->send_message(ctx, v.destinationProc, &v, sizeof(v), POOL_POOL_WAKEUP_SENT_DOWN);
        log_psana_mapper.debug("proc %llx: send POOL_POOL_WAKEUP_SENT_DOWN hops %d to worker %llx",
                               local_proc.id, v.hops, v.destinationProc.id);
      }
      
      if(r.hops < NUM_TASK_POOL_MAPPERS) {
        v = r;
        v.sourceProc = local_proc;
        v.destinationProc = r.destinationProc;
        runtime->send_message(ctx, v.destinationProc, &v, sizeof(v), POOL_POOL_WAKEUP_SENT_UP);
        log_psana_mapper.debug("proc %llx: send POOL_POOL_WAKEUP_SENT_UP hops %d to worker %llx",
                               local_proc.id, v.hops, v.destinationProc.id);
     }
      pendingRequestIt = pendingRequest.erase(pendingRequestIt);
      numAvailableTasks--;
      
    } else {
      pendingRequestIt++;
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
  
  if(mapperCategory == TASK_POOL) {
    
    log_psana_mapper.debug("proc %llx: select_tasks_to_map readyTasks size %ld",
                           local_proc.id,
                           input.ready_tasks.size());
    
    bool mapped = mapTaskPoolTasks(ctx, input, output);
    
    unsigned numUnmappedWorkerTasks;
    mapped |= relocateWorkerTasks(ctx, input, output, numUnmappedWorkerTasks);
    
    wakeUpWorkers(ctx, input, output, numUnmappedWorkerTasks);
    
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
  // task completion request
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
  } else if(mapperCategory == TASK_POOL) {
    log_psana_mapper.debug("proc %llx: map_task task pool maps %s to itself",
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

