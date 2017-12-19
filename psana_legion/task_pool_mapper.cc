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

#include "task_pool_mapper.h"



/*
 Goal: write a distributed task pool to serve a set of worker processors.
 
 There are n mappers running for n processors.
 Mappers send and respond to these kinds of messages.
 
 WORKER_POOL_STEAL_REQUEST
 WORKER_POOL_STEAL_ACK
 WORKER_POOL_STEAL_NACK
 POOL_WORKER_WAKEUP
 POOL_POOL_FORWARD_STEAL
 POOL_POOL_FORWARD_STEAL_SUCCESS
 
 Message payloads, also used as request record:
 { unique-id, source, destination, worker, numTasks, hops }
 
 Algorithm:
 
 Initially, categorize the mappers into POOL and WORKER type.
 Each worker sends initially WORKER_POOL_STEAL_REQUEST to pools.
 Each worker subsequently sends WORKER_POOL_STEAL_REQUEST to pools whenever it
 completes a task.
 
 Pool handles WORKER_POOL_STEAL_REQUEST.  If work is available relocate one task
 and send WORKER_POOL_STEAL_ACK to the requestor.
 If work is not available store the request on a failed-list, and forward
 the request by sending POOL_POOL_FORWARD_STEAL to a different pool mapper.
 
 Pool handles POOL_POOL_FORWARD_STEAL.  If work is available relocate one task
 and send POOL_POOL_FORWARD_STEAL_SUCCESS to the source processor.
 If work is not available store the requeston the failed-list and check
 the number of hops; if hops < num pools then forward POOL_POOL_FORWARD_STEAL to
 another pool with an incremented hop count.  If hops == num pools - 1 send
 POOL_WORKER_STEAL_NACK to the requestor.
 
 Pool handles POOL_POOL_FORWARD_STEAL_SUCCESS.  Delete the stored request by
 unique id and send POOL_POOL_FORWARD_STEAL_SUCCESS to request.source.  By
 transitivity this will delete all the way back to the original source.
 
 Pool sometimes spontaneously receives new work.  For each stored request on
 the failed list send POOL_WORKER_WAKEUP to request.worker.
 
 Worker handles POOL_WORKER_WAKEUP.  If workload is below threshold then send
 WORKER_POOL_STEAL_REQUEST to pools.
 
 
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
 original worker w. Worker w sends WORKER_POOL_STEAL_REQUEST to pool.
 4. Same as 2 but while POOL_POOL_FORWARD_STEAL is being forwarded another
 WORKER_POOL_STEAL_REQUEST comes in and causes another set of messages to happen
 in parallel with the first set.
 5. Same as 3 but multiple pools spontaneously get work at the same time.
 6. Same as 3 but a new POOL_POOL_FORWARD_STEAL comes in immediately after a
 pool issues the first POOL_WORKER_WAKEUP.
 7. Same as 3 but a new POOL_POOL_FORWARD_STEAL sequence is in progress before any
 pool issues the first POOL_WORKER_WAKEUP.
 8. Multiple unsatisfied worker requests lead to multiple simultaneous chains of
 POOL_POOL_FORWARD_STEAL.
 
 
 Implementation in mapper API:
 data structure deque<request record> in each mapper
 no threadsafety issues
 
 First entered in select_steal_targets, each worker sends WORKER_POOL_STEAL_REQUEST
 to pool.  On task completion worker sends WORKER_POOL_STEAL_REQUEST if conditions
 are satisfied: task queue size is below threshold, and there is no outstanding
 request still in flight.
 
 Worker handles POOL_WORKER_STEAL_ACK and POOL_WORKER_STEAL_NACK by noting that
 steal request is no longer outstanding.
 
 Pool handles WORKER_POOL_STEAL_REQUEST. If there is work available push tasks
 onto a send_queue and trigger select_tasks_to_map by a MapperEvent.  If a request
 is less than fully satisfied put in on the failed list.
 
 Pool handles POOL_POOL_FORWARD_STEAL.  Same as WORKER_POOL_STEAL_REQUEST except
 that if work is available send POOL_POOL_FORWARD_STEAL_SUCCESS to the source of
 the message.
 
 select_tasks_to_map() for pool mappers:
 * Copy available worker tasks to worker_ready_queue.  Map nonworker tasks locally.
 * Send the entries from the send_queueu, and send POOL_WORKER_STEAL_ACK to each
 worker that sent a request.
 * If work is still available send POOL_WORKER_WAKEUP to all workers in the failed list.
 
 select_tasks_to_map() for worker mappers:
 map all worker tasks to local proc.
 
 Pool handles POOL_POOL_FORWARD_STEAL_SUCCESS.  Delete request from deque.  If
 hops > 0 send POOL_POOL_FORWARD_STEAL_SUCCESS to immediate sender.
 
 Worker handles POOL_WORKER_WAKEUP.  Worker sends WORKER_POOL_STEAL_REQUEST if
 workload is below threshold.
 
 
 */

#include <map>
#include <random>
#include <time.h>
#include <vector>
#include <deque>

#include "default_mapper.h"


#define VERBOSE_DEBUG 1

using namespace Legion;
using namespace Legion::Mapping;

static const char* ANALYSIS_TASK_NAMES[] = {
  "psana_legion.analyze",
  "psana_legion.analyze_leaf",
  "jump"
};

static const int NUM_RANKS_PER_TASK_POOL = 4;
static const int TASKS_PER_STEALABLE_SLICE = 1;
static const int MIN_TASKS_PER_PROCESSOR = 2;

typedef enum {
  TASK_POOL,
  WORKER,
  IO,
  LEGION_CPU
} MapperCategory;

typedef enum {
  WORKER_POOL_STEAL_REQUEST = 1,
  POOL_POOL_FORWARD_STEAL,
  POOL_WORKER_STEAL_ACK,
  POOL_POOL_FORWARD_STEAL_SUCCESS,
  POOL_WORKER_STEAL_NACK,
  POOL_WORKER_WAKEUP,
} MessageType;

typedef struct {
  unsigned id;
  Processor sourceProc;
  Processor destinationProc;
  Processor thiefProc;
  unsigned numTasks;
  unsigned hops;
} Request;


static LegionRuntime::Logger::Category log_task_pool_mapper("task_pool_mapper");


///
/// Mapper
///



class TaskPoolMapper : public DefaultMapper
{
public:
  TaskPoolMapper(MapperRuntime *rt,
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
  Processor nearestTaskPoolProc;
  Processor nearestIOProc;
  Processor nearestLegionCPUProc;
  std::map<std::pair<LogicalRegion,Memory>,PhysicalInstance> local_instances;
  typedef long long Timestamp;
  int taskWorkloadSize;
  MapperRuntime *runtime;
  MapperEvent defer_select_tasks_to_map;
  std::set<const Task*> worker_ready_queue;
  typedef std::vector<std::pair<Request, std::vector<const Task*> > > SendQueue;
  SendQueue send_queue;
  std::deque<Request> failed_requests;
  unsigned numUniqueIds;
  bool stealRequestOutstanding;
  
  Timestamp timeNow() const;
  unsigned uniqueId();
  void categorizeMappers();
  std::string taskDescription(const Legion::Task& task);
  std::string prolog(const char* function) const;
  bool isAnalysisTask(const Legion::Task& task);
  void slice_task(const MapperContext      ctx,
                  const Task&              task,
                  const SliceTaskInput&    input,
                  SliceTaskOutput&   output);
  void decompose_points(const Rect<1, coord_t> &point_rect,
                        const std::vector<Processor> &targets,
                        const Point<1, coord_t> &num_blocks,
                        std::vector<TaskSlice> &slices);
  void handleStealRequest(const MapperContext          ctx,
                          Request r,
                          MessageType messageType);
  void forwardStealRequest(const MapperContext          ctx,
                           Request r);
  void wakeUpWorkers(const MapperContext          ctx,
                     unsigned numAvailableTasks);
  bool alreadyQueued(const Task* task);
  bool filterInputReadyTasks(const SelectMappingInput&    input,
                             SelectMappingOutput&   output);
  bool sendSatisfiedTasks(const MapperContext          ctx,
                          SelectMappingOutput&   output);
  char* processorKindString(unsigned kind) const;
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
  void maybeSendStealRequest(MapperContext ctx, Processor target);
  void triggerSelectTasksToMap(const MapperContext ctx);
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
  void handle_POOL_POOL_FORWARD_STEAL_SUCCESS(const MapperContext ctx,
                                              const MapperMessage& message);
  virtual void handle_message(const MapperContext           ctx,
                              const MapperMessage&          message);
  
  const char* get_mapper_name(void) const { return "task_pool_mapper"; }
  MapperSyncModel get_mapper_sync_model(void) const {
    return SERIALIZED_REENTRANT_MAPPER_MODEL;
  }
};

//--------------------------------------------------------------------------
std::string describeProcId(long long procId)
//--------------------------------------------------------------------------
{
 char buffer[128];
  unsigned nodeId = procId >> 40;
  unsigned pId = procId & 0xffffffffff;
  sprintf(buffer, "node %x proc %x", nodeId, pId);
  return std::string(buffer);
}


//--------------------------------------------------------------------------
TaskPoolMapper::TaskPoolMapper(MapperRuntime *rt,
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
  log_task_pool_mapper.info("%s constructor", describeProcId(local_proc.id).c_str());
  categorizeMappers();
  
  rng = std::mt19937(rd());    // random-number engine used (Mersenne-Twister in this case)
  uni = std::uniform_int_distribution<int>(0, (int)task_pool_procs.size() - 1); // guaranteed unbiased
  runtime = rt;
  numUniqueIds = 0;
  stealRequestOutstanding = false;
  taskWorkloadSize = 0;
  log_task_pool_mapper.info("%lld # %s taskWorkloadSize %d mapper category %d %s",
                            timeNow(), describeProcessorId(local_proc.id).c_str(),
                            taskWorkloadSize, mapperCategory,
                            processorKindString(local_proc.kind()));
}



//--------------------------------------------------------------------------
std::string TaskPoolMapper::prolog(const char* function) const
//--------------------------------------------------------------------------
{
 char buffer[512];
  assert(strlen(function) < sizeof(buffer));
  sprintf(buffer, "%lld %s%s: %s",
          timeNow(), describeProcId(local_proc.id).c_str(),
          (mapperCategory == WORKER ? "(W)" : (mapperCategory == TASK_POOL ? "(T)" :
            (mapperCategory == IO ? "(I)" : (mapperCategory == LEGION_CPU ? "(L)" : "")))),
          function);
  return std::string(buffer);
}


//--------------------------------------------------------------------------
void TaskPoolMapper::maybeSendStealRequest(MapperContext ctx, Processor target)
//--------------------------------------------------------------------------
{
  assert(mapperCategory == WORKER);
  if(taskWorkloadSize < MIN_TASKS_PER_PROCESSOR && !stealRequestOutstanding) {
    unsigned numTasks = MIN_TASKS_PER_PROCESSOR - taskWorkloadSize;
    Request r = { uniqueId(), local_proc, target, local_proc, numTasks, 0 };
    log_task_pool_mapper.debug("%s "
                               "send WORKER_POOL_STEAL_REQUEST "
                               "id %d numTasks %d to %s",
                               prolog(__FUNCTION__).c_str(),
                               r.id, numTasks, (describeProcId(target.id).c_str()));
    stealRequestOutstanding = true;
    runtime->send_message(ctx, target, &r, sizeof(r), WORKER_POOL_STEAL_REQUEST);
  } else {
    if(taskWorkloadSize < MIN_TASKS_PER_PROCESSOR && stealRequestOutstanding) {
      log_task_pool_mapper.debug("%s "
                                 "cannot send because stealRequestOutstanding",
                                 prolog(__FUNCTION__).c_str());
    }
  }
}


//--------------------------------------------------------------------------
unsigned TaskPoolMapper::uniqueId()
//--------------------------------------------------------------------------
{
  unsigned nodeId = local_proc.id >> 40;
  unsigned pId = local_proc.id & 0xffffffffff;
  unsigned maxNodeId = 128 * 1024;
  unsigned maxProcId = 128;
  unsigned result = numUniqueIds++ * (maxNodeId * maxProcId) + (nodeId * pId);
  return result;
}

//--------------------------------------------------------------------------
void TaskPoolMapper::triggerSelectTasksToMap(const MapperContext ctx)
//--------------------------------------------------------------------------
{
  if(defer_select_tasks_to_map.exists()){
    MapperEvent temp_event = defer_select_tasks_to_map;
    defer_select_tasks_to_map = MapperEvent();
    runtime->trigger_mapper_event(ctx, temp_event);
  }
}

//--------------------------------------------------------------------------
void TaskPoolMapper::handle_WORKER_POOL_STEAL_REQUEST(const MapperContext ctx,
                                                      const MapperMessage& message)
//--------------------------------------------------------------------------
{
  assert(mapperCategory == TASK_POOL);
  Request r = *(Request*)message.message;
  log_task_pool_mapper.debug("%s handle_WORKER_POOL_STEAL_REQUEST id %d "
                             "numTasks %d from worker %s",
                             prolog(__FUNCTION__).c_str(), r.id, r.numTasks,
                             (describeProcId(r.thiefProc.id).c_str()));
  handleStealRequest(ctx, r, (MessageType)message.kind);
}

//--------------------------------------------------------------------------
void TaskPoolMapper::handle_POOL_WORKER_STEAL_ACK(const MapperContext ctx,
                                                  const MapperMessage& message)
//--------------------------------------------------------------------------
{
  assert(mapperCategory == WORKER);
  stealRequestOutstanding = false;
  Request r = *(Request*)message.message;
  taskWorkloadSize += r.numTasks;
  log_task_pool_mapper.debug("%s handle_POOL_WORKER_STEAL_ACK id %d from "
                             "proc %s numTasks %d taskWorkloadSize %d",
                             prolog(__FUNCTION__).c_str(),
                             r.id,
                             (describeProcId(r.sourceProc.id).c_str()),
                             r.numTasks,
                             taskWorkloadSize);
}

//--------------------------------------------------------------------------
void TaskPoolMapper::handle_POOL_WORKER_STEAL_NACK(const MapperContext ctx,
                                                   const MapperMessage& message)
//--------------------------------------------------------------------------
{
  assert(mapperCategory == WORKER);
  stealRequestOutstanding = false;
  Request r = *(Request*)message.message;
  log_task_pool_mapper.debug("%s handle_POOL_WORKER_STEAL_NACK id %d from "
                             "proc %s taskWorkloadSize %d",
                             prolog(__FUNCTION__).c_str(),
                             r.id,
                             (describeProcId(r.sourceProc.id).c_str()),
                             taskWorkloadSize);
}

//--------------------------------------------------------------------------
void TaskPoolMapper::handle_POOL_WORKER_WAKEUP(const MapperContext ctx,
                                               const MapperMessage& message)
//--------------------------------------------------------------------------
{
  assert(mapperCategory == WORKER);
  Request r = *(Request*)message.message;
  log_task_pool_mapper.debug("%s handle_POOL_WORKER_WAKEUP id %d from %s",
                             prolog(__FUNCTION__).c_str(), r.id,
                             (describeProcId(r.sourceProc.id).c_str()));
  maybeSendStealRequest(ctx, r.sourceProc);
}

//--------------------------------------------------------------------------
void TaskPoolMapper::handle_POOL_POOL_FORWARD_STEAL(const MapperContext ctx,
                                                    const MapperMessage& message)
//--------------------------------------------------------------------------
{
  assert(mapperCategory == TASK_POOL);
  Request r = *(Request*)message.message;
  log_task_pool_mapper.debug("%s handle_POOL_POOL_FORWARD_STEAL id %d "
                             "hops %d numTasks %d from worker %s",
                             prolog(__FUNCTION__).c_str(),
                             r.id, r.hops, r.numTasks, 
                             (describeProcId(r.thiefProc.id).c_str()));
  handleStealRequest(ctx, r, (MessageType)message.kind);
}

//--------------------------------------------------------------------------
void TaskPoolMapper::handle_POOL_POOL_FORWARD_STEAL_SUCCESS(const MapperContext ctx,
                                                            const MapperMessage& message)
//--------------------------------------------------------------------------
{
  assert(mapperCategory == TASK_POOL);
  Request r = *(Request*)message.message;
  log_task_pool_mapper.debug("%s handle_POOL_POOL_FORWARD_STEAL_SUCCESS "
                             "id %d hops %d numTasks %d from %s",
                             prolog(__FUNCTION__).c_str(),
                             r.id, r.hops, r.numTasks, 
                             (describeProcId(r.sourceProc.id).c_str()));
  
  for(std::deque<Request>::iterator it = failed_requests.begin();
      it != failed_requests.end(); ) {
    Request v = *it;
    if(r.id == v.id && r.hops == v.hops + 1) {
      it = failed_requests.erase(it);
      if(v.hops > 0) {
        log_task_pool_mapper.debug("%s send POOL_POOL_FORWARD_STEAL_SUCCESS "
                                   "id %d hops %d numTasks %d to %s",
                                   prolog(__FUNCTION__).c_str(),
                                   v.id, v.hops, v.numTasks, 
                                   (describeProcId(v.sourceProc.id).c_str()));
        runtime->send_message(ctx, v.sourceProc, &v, sizeof(v), POOL_POOL_FORWARD_STEAL_SUCCESS);
      }
      // Once we find it we are done
      break;
    } else {
      it++;
    }
  }
}

//--------------------------------------------------------------------------
void TaskPoolMapper::handle_message(const MapperContext ctx,
                                    const MapperMessage& message)
//--------------------------------------------------------------------------
{
  assert(mapperCategory == TASK_POOL || mapperCategory == WORKER);
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
    case POOL_POOL_FORWARD_STEAL_SUCCESS:
      handle_POOL_POOL_FORWARD_STEAL_SUCCESS(ctx, message);
      break;
    default: assert(false);
  }
}

//--------------------------------------------------------------------------
TaskPoolMapper::Timestamp TaskPoolMapper::timeNow() const
//--------------------------------------------------------------------------
{
  return Realm::Clock::current_time_in_nanoseconds();
}


//--------------------------------------------------------------------------
void TaskPoolMapper::categorizeMappers()
//--------------------------------------------------------------------------
{
  unsigned count = 0;
  unsigned ioProcCount = 0;
  unsigned legionProcCount = 0;
  Processor recentTaskPoolProc;
  Processor recentIOProc;
  Processor recentLegionCPUProc;
  
  for(std::map<Processor, Memory>::iterator it = proc_sysmems.begin();
      it != proc_sysmems.end(); it++) {
    Processor processor = it->first;
    switch(processor.kind()) {
      case TOC_PROC:
        break;
      case LOC_PROC:
        if(processor == local_proc) {
          mapperCategory = LEGION_CPU;
        }
        legionProcCount++;
        recentLegionCPUProc = processor;
        break;
      case UTIL_PROC:
        break;
      case IO_PROC:
        if(processor == local_proc) {
          mapperCategory = IO;
        }
        ioProcCount++;
        recentIOProc = processor;
        break;
      case PROC_GROUP:
        break;
      case PROC_SET:
        break;
      case OMP_PROC:
        break;
      case PY_PROC:
        if(count++ % NUM_RANKS_PER_TASK_POOL == 0) {
          task_pool_procs.push_back(processor);
          if(processor == local_proc) {
            mapperCategory = TASK_POOL;
          }
          recentTaskPoolProc = processor;
          nearestTaskPoolProc = recentTaskPoolProc;
          nearestIOProc = recentIOProc;
          nearestLegionCPUProc = recentLegionCPUProc;
        } else {
          worker_procs.push_back(processor);
          if(processor == local_proc) {
            mapperCategory = WORKER;
          }
          nearestTaskPoolProc = recentTaskPoolProc;
          nearestIOProc = recentIOProc;
          nearestLegionCPUProc = recentLegionCPUProc;
        }
        break;
      default: assert(false);
    }
  }
  
  log_task_pool_mapper.debug("%s %ld task pool, "
                             "%ld worker processors, %u io processors, "
                             "%u legion_cpu processors",
                             prolog(__FUNCTION__).c_str(),
                             task_pool_procs.size(),
                             worker_procs.size(),
                             ioProcCount, legionProcCount);
}



//--------------------------------------------------------------------------
std::string TaskPoolMapper::taskDescription(const Legion::Task& task)
//--------------------------------------------------------------------------
{
  char buffer[512];
  sprintf(buffer, "<%s:%llx>", task.get_task_name(), task.get_unique_id());
  return std::string(buffer);
}


//--------------------------------------------------------------------------
bool TaskPoolMapper::isAnalysisTask(const Legion::Task& task)
//--------------------------------------------------------------------------
{
  int numAnalysisTasks = sizeof(ANALYSIS_TASK_NAMES) / sizeof(ANALYSIS_TASK_NAMES[0]);
  for(int i = 0; i < numAnalysisTasks; ++i) {
    if(!strcmp(task.get_task_name(), ANALYSIS_TASK_NAMES[i])) {
      return true;
    }
  }
  return false;
}


//--------------------------------------------------------------------------
void TaskPoolMapper::decompose_points(const Rect<1, coord_t> &point_rect,
                                      const std::vector<Processor> &targets,
                                      const Point<1, coord_t> &num_blocks,
                                      std::vector<TaskSlice> &slices)
//--------------------------------------------------------------------------
{
  long long num_points = point_rect.hi - point_rect.lo + Point<1, coord_t>(1);
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
void TaskPoolMapper::slice_task(const MapperContext      ctx,
                                const Task&              task,
                                const SliceTaskInput&    input,
                                SliceTaskOutput&   output)
//--------------------------------------------------------------------------
{
  
  if(isAnalysisTask(task)){
    log_task_pool_mapper.debug("%s task %s target proc %s proc_kind %d",
                               prolog(__FUNCTION__).c_str(),
                               taskDescription(task).c_str(),
                               (describeProcId(task.target_proc.id).c_str()),
                               task.target_proc.kind());
    assert(input.domain.get_dim() == 1);
    
    Rect<1, coord_t> point_rect = input.domain;
    Point<1, coord_t> num_blocks(point_rect.volume() / TASKS_PER_STEALABLE_SLICE);
    decompose_points(point_rect, task_pool_procs, num_blocks, output.slices);
    
  } else {
    log_task_pool_mapper.debug("%s pass %s to default mapper",
                               prolog(__FUNCTION__).c_str(),
                               taskDescription(task).c_str());
    this->DefaultMapper::slice_task(ctx, task, input, output);
  }
}


//--------------------------------------------------------------------------
void TaskPoolMapper::handleStealRequest(const MapperContext          ctx,
                                        Request r,
                                        MessageType messageType)
//--------------------------------------------------------------------------
{
  // See if we can satisfy this request
  if (!worker_ready_queue.empty())
  {
    // Grab the tasks that we are going to steal
    std::vector<const Task*> tasks;
    std::set<const Task*>::const_iterator to_steal = worker_ready_queue.begin();
    
    unsigned numStolen = 0;
    while ((r.numTasks > 0) && (to_steal != worker_ready_queue.end()))
    {
      tasks.push_back(*to_steal);
      to_steal = worker_ready_queue.erase(to_steal);
      r.numTasks--;
      numStolen++;
    }
    Request v = r;
    v.numTasks = numStolen;
    send_queue.push_back(std::make_pair(v, tasks));
    
    if(messageType == POOL_POOL_FORWARD_STEAL) {
      Request v = r;
      v.sourceProc = local_proc;
      v.destinationProc = r.sourceProc;
      log_task_pool_mapper.debug("%s send "
                                 "POOL_POOL_FORWARD_STEAL_SUCCESS id %d "
                                 "hops %d numTasks %d to %s",
                                 prolog(__FUNCTION__).c_str(),
                                 v.id, v.hops, v.numTasks, 
                                 (describeProcId(v.destinationProc.id).c_str()));
      runtime->send_message(ctx, v.destinationProc, &v, sizeof(v), POOL_POOL_FORWARD_STEAL_SUCCESS);
    }
    if(r.numTasks > 0) {
      failed_requests.push_back(r);
    }
    triggerSelectTasksToMap(ctx);
  }
  else
  {
    failed_requests.push_back(r);
    forwardStealRequest(ctx, r);
  }
}

//--------------------------------------------------------------------------
void TaskPoolMapper::forwardStealRequest(const MapperContext          ctx,
                                         Request r)
//--------------------------------------------------------------------------
{
  if(r.hops < task_pool_procs.size() - 1) {
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
    log_task_pool_mapper.debug("%s send POOL_POOL_FORWARD_STEAL id %d "
                               "hops %d numTasks %d to %s",
                               prolog(__FUNCTION__).c_str(),
                               v.id, v.hops, v.numTasks, 
                               (describeProcId(target.id).c_str()));
    runtime->send_message(ctx, target, &v, sizeof(v), POOL_POOL_FORWARD_STEAL);
  } else {
    Request v = r;
    v.sourceProc = local_proc;
    v.destinationProc = r.thiefProc;
    log_task_pool_mapper.debug("%s send POOL_WORKER_STEAL_NACK id %d to %s",
                               prolog(__FUNCTION__).c_str(),
                               v.id, (describeProcId(r.thiefProc.id).c_str()));
    runtime->send_message(ctx, r.thiefProc, &v, sizeof(v), POOL_WORKER_STEAL_NACK);
  }
}

//--------------------------------------------------------------------------
void TaskPoolMapper::wakeUpWorkers(const MapperContext          ctx,
                                   unsigned numAvailableTasks)
//--------------------------------------------------------------------------
{
  std::deque<Request>::iterator pendingRequestIt = failed_requests.begin();
  std::map<Processor, Request> to_notify;
  while(pendingRequestIt != failed_requests.end() && numAvailableTasks > 0) {
    Request r = *pendingRequestIt;
    log_task_pool_mapper.debug("%s request id %d "
                               "workerId %s numTasks %d",
                               prolog(__FUNCTION__).c_str(),
                               r.id, (describeProcId(r.thiefProc.id).c_str()),
                               r.numTasks);
    to_notify[r.thiefProc] = r;
    pendingRequestIt = failed_requests.erase(pendingRequestIt);
    numAvailableTasks--;
  }
  for (std::map<Processor, Request>::const_iterator it = to_notify.begin();
       it != to_notify.end(); it++)
  {
    Request v = it->second;
    v.sourceProc = local_proc;
    v.destinationProc = it->first;
    log_task_pool_mapper.debug("%s send POOL_WORKER_WAKEUP id %d to worker %s",
                               prolog(__FUNCTION__).c_str(),
                               v.id, (describeProcId(v.destinationProc.id).c_str()));
    
    runtime->send_message(ctx, v.destinationProc, &v, sizeof(v), POOL_WORKER_WAKEUP);
  }
}


//--------------------------------------------------------------------------
bool TaskPoolMapper::alreadyQueued(const Task* task)
//--------------------------------------------------------------------------
{
  if(worker_ready_queue.find(task) != worker_ready_queue.end()) {
    return true;
  }
  //TODO use a set here instead of searching over the send queue
  for(SendQueue::iterator sqIt = send_queue.begin();
      sqIt != send_queue.end(); sqIt++) {
    std::vector<const Task*> tasks = sqIt->second;
    for(std::vector<const Task*>::iterator taskIt = tasks.begin();
        taskIt != tasks.end(); taskIt++) {
      if(*taskIt == task) {
        return true;
      }
    }
  }
  return false;
}

//--------------------------------------------------------------------------
bool TaskPoolMapper::filterInputReadyTasks(const SelectMappingInput&    input,
                                           SelectMappingOutput&   output)
//--------------------------------------------------------------------------
{
  assert(mapperCategory == TASK_POOL);
  bool mapped = false;
  // Copy over any new tasks into our worker ready queue
  for (std::list<const Task*>::const_iterator it = input.ready_tasks.begin();
       it != input.ready_tasks.end(); it++)
  {
    const Task* task = *it;
    bool mapLocally = taskWorkloadSize < MIN_TASKS_PER_PROCESSOR || !isAnalysisTask(*task);
    if(mapLocally) {
      if (isAnalysisTask(*task))
      {
        std::set<const Task*>::iterator finder = worker_ready_queue.find(task);
        if (finder == worker_ready_queue.end())
        {
          // If it's already in the send queue then we can't claim it
          if (alreadyQueued(task))
            continue;
          // Else not seen before so we can claim it
        }
        else // Not in send queue yet so we can claim it
          worker_ready_queue.erase(finder);
      }
      output.map_tasks.insert(task);
      mapped = true;
      taskWorkloadSize++;
      log_task_pool_mapper.debug("%s pool selects %s for local mapping",
                                 prolog(__FUNCTION__).c_str(),
                                 taskDescription(*task).c_str());
    } else {
      if(isAnalysisTask(*task) && !alreadyQueued(task)) {
        worker_ready_queue.insert(task);
        log_task_pool_mapper.debug("%s pool %s on worker_ready_queue",
                                   prolog(__FUNCTION__).c_str(),
                                   taskDescription(*task).c_str());
      }
    }
  }
  return mapped;
}

//--------------------------------------------------------------------------
bool TaskPoolMapper::sendSatisfiedTasks(const MapperContext          ctx,
                                        SelectMappingOutput&   output)
//--------------------------------------------------------------------------
{
  assert(mapperCategory == TASK_POOL);
  bool mapped = false;
  // Send any tasks that we satisfied
  while (!send_queue.empty())
  {
    std::vector<Request> messages_to_send;
    for(SendQueue::iterator it = send_queue.begin();
        it != send_queue.end(); it++) {
      Request r = it->first;
      Processor processor = r.thiefProc;
      std::vector<const Task*> tasks = it->second;
      
      for(std::vector<const Task*>::const_iterator taskIt = tasks.begin();
          taskIt != tasks.end(); ++taskIt) {
        const Task* task = *taskIt;
        output.relocate_tasks[task] = processor;
        mapped = true;
        log_task_pool_mapper.info("# %lld p %s remapping %s to %llx",
                                  timeNow(),
                                  describeProcId(local_proc.id).c_str(),
                                  taskDescription(*task).c_str(),
                                  processor.id);
        log_task_pool_mapper.debug("%s relocating "
                                   "%s to %s",
                                   prolog(__FUNCTION__).c_str(),
                                   taskDescription(*task).c_str(),
                                   (describeProcId(processor.id).c_str()));
      }
      
      // Send an ack to the worker
      // Defer sending the messages until we are done with the loop
      // to avoid preemption that could invalidate our iterator
      messages_to_send.resize(messages_to_send.size() + 1);
      Request &v = messages_to_send.back();
      v = r;
      v.sourceProc = local_proc;
      v.destinationProc = processor;
    }
    
    send_queue.clear();
    for (unsigned idx = 0; idx < messages_to_send.size(); idx++) {
      Request &v = messages_to_send[idx];
      log_task_pool_mapper.debug("%s send POOL_WORKER_STEAL_ACK id %d"
                                 " numTasks %d to %s",
                                 prolog(__FUNCTION__).c_str(),
                                 v.id, v.numTasks, 
                                 (describeProcId(v.thiefProc.id).c_str()));
      runtime->send_message(ctx, v.destinationProc, &v, sizeof(v), POOL_WORKER_STEAL_ACK);
    }
  }
  return mapped;
}

//--------------------------------------------------------------------------
char* TaskPoolMapper::processorKindString(unsigned kind) const
//--------------------------------------------------------------------------
{
  switch(kind) {
    case TOC_PROC:
      return (char*)"TOC_PROC";
      break;
    case LOC_PROC:
      return (char*)"LOC_PROC";
      break;
    case UTIL_PROC:
      return (char*)"UTIL_PROC";
      break;
    case IO_PROC:
      return (char*)"IO_PROC";
      break;
    case PROC_GROUP:
      return (char*)"PROC_GROUP";
      break;
    case PROC_SET:
      return (char*)"PROC_SET";
      break;
    case OMP_PROC:
      return (char*)"OMP_PROC";
      break;
    case PY_PROC:
      return (char*)"PY_PROC";
      break;
    case NO_KIND:
      return (char*)"NO_KIND";
      break;
    default:
      log_task_pool_mapper.debug("%s processor kind %d",
                                 prolog(__FUNCTION__).c_str(), kind);
      assert(false);
  }
}

//--------------------------------------------------------------------------
void TaskPoolMapper::select_tasks_to_map(const MapperContext          ctx,
                                         const SelectMappingInput&    input,
                                         SelectMappingOutput&   output)
//--------------------------------------------------------------------------
{
#if VERBOSE_DEBUG
  for(std::list<const Task*>::const_iterator it = input.ready_tasks.begin();
      it != input.ready_tasks.end(); it++) {
    log_task_pool_mapper.debug("%s input.ready_tasks %s",
                               prolog(__FUNCTION__).c_str(),
                               taskDescription(**it).c_str());
  }
#endif
  
  if(defer_select_tasks_to_map.exists()) {
    triggerSelectTasksToMap(ctx);
  }
  
  if(mapperCategory == TASK_POOL) {
    
    log_task_pool_mapper.debug("%s readyTasks size %ld",
                               prolog(__FUNCTION__).c_str(),
                               input.ready_tasks.size());
    
    bool mapped = filterInputReadyTasks(input, output);
    
    
    // Notify any failed requests that we have work
    if (!worker_ready_queue.empty())
      wakeUpWorkers(ctx, (unsigned)worker_ready_queue.size());
    
    mapped |= sendSatisfiedTasks(ctx, output);
    assert(send_queue.empty());
    
    if (!mapped && !input.ready_tasks.empty()) {
      log_task_pool_mapper.debug("%s trigger subsequent invocation",
                                 prolog(__FUNCTION__).c_str());
      if (!defer_select_tasks_to_map.exists()) {
        defer_select_tasks_to_map = runtime->create_mapper_event(ctx);
      }
      output.deferral_event = defer_select_tasks_to_map;
    }
    
  } else {// worker, io, legion_cpu mapper
    
    for (std::list<const Task*>::const_iterator it = input.ready_tasks.begin();
         it != input.ready_tasks.end(); it++) {
      const Task* task = *it;
      VariantInfo chosen = default_find_preferred_variant(*task, ctx,
                                                          true/*needs tight bound*/, false/*cache*/, Processor::NO_KIND);
      if(chosen.proc_kind == local_proc.kind()) {
        log_task_pool_mapper.debug("%s %s selects %s",
                                   prolog(__FUNCTION__).c_str(),
                                   processorKindString(local_proc.kind()),
                                   taskDescription(*task).c_str());
        output.map_tasks.insert(task);
      } else {
        log_task_pool_mapper.debug("%s %s relocates %s to %s",
                                   prolog(__FUNCTION__).c_str(),
                                   processorKindString(local_proc.kind()),
                                   taskDescription(*task).c_str(),
                                   processorKindString(task->target_proc.kind()));
       switch(chosen.proc_kind) {
          case Realm::Processor::IO_PROC:
            output.relocate_tasks[task] = nearestIOProc;
            break;
          case Realm::Processor::LOC_PROC:
            output.relocate_tasks[task] = nearestLegionCPUProc;
            break;
          default: assert(false);
        }
      }
    }
  }
  
#if VERBOSE_DEBUG
  for(std::set<const Task*>::const_iterator mapIt = output.map_tasks.begin();
      mapIt != output.map_tasks.end(); mapIt++) {
    const Task* task = *mapIt;
    log_task_pool_mapper.debug("%s output.map_tasks %s",
                               prolog(__FUNCTION__).c_str(),
                               taskDescription(*task).c_str());
  }
  for(std::map<const Task*, Processor>::const_iterator relIt = output.relocate_tasks.begin();
      relIt != output.relocate_tasks.end(); ++relIt) {
    const Task* task = relIt->first;
    Processor p = relIt->second;
    log_task_pool_mapper.debug("%s output.relocate_Tasks %s %s",
                               prolog(__FUNCTION__).c_str(),
                               taskDescription(*task).c_str(), (int)(p.id & 0xff));
  }
#endif
  
}




//--------------------------------------------------------------------------
void TaskPoolMapper::select_steal_targets(const MapperContext         ctx,
                                          const SelectStealingInput&  input,
                                          SelectStealingOutput& output)
//--------------------------------------------------------------------------
{
  if(mapperCategory == WORKER) {
    maybeSendStealRequest(ctx, nearestTaskPoolProc);
  }
}


//------------------------------------------------------------------------------
Memory TaskPoolMapper::get_associated_sysmem(Processor proc)
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
void TaskPoolMapper::map_task_array(const MapperContext ctx,
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
    log_task_pool_mapper.error("TASK_POOL mapper failed to allocate instance");
    assert(false);
  }
  instances.push_back(result);
  local_instances[key] = result;
}

//--------------------------------------------------------------------------
void TaskPoolMapper::report_profiling(const MapperContext      ctx,
                                      const Task&              task,
                                      const TaskProfilingInfo& input)
//--------------------------------------------------------------------------
{
  // task completion request
  taskWorkloadSize--;
  log_task_pool_mapper.info("%lld proc %llx: report_profiling # %s %s taskWorkloadSize %d",
                            timeNow(),
                            describeProcessorId(local_proc.id).c_str(),
                            (int)(local_proc.id & 0xff),
                            taskDescription(task).c_str(),
                            taskWorkloadSize);
  if(mapperCategory == WORKER) {
    maybeSendStealRequest(ctx, nearestTaskPoolProc);
  }
}

//--------------------------------------------------------------------------
void TaskPoolMapper::map_task(const MapperContext      ctx,
                              const Task&              task,
                              const MapTaskInput&      input,
                              MapTaskOutput&     output)
//--------------------------------------------------------------------------
{
  
  VariantInfo chosen = default_find_preferred_variant(task, ctx,
                                                      true/*needs tight bound*/, false/*cache*/, Processor::NO_KIND);
  output.chosen_variant = chosen.variant;
  output.task_priority = 0;
  output.postmap_task = false;
  output.target_procs.push_back(local_proc);

  if(task.orig_proc == local_proc) {
    taskWorkloadSize++;
    log_task_pool_mapper.debug("%s maps self task %s"
                               " taskWorkloadSize %d",
                               prolog(__FUNCTION__).c_str(),
                               taskDescription(task).c_str(),
                               taskWorkloadSize);
  } else {
    if(mapperCategory != WORKER) {
      taskWorkloadSize++;
    }
    log_task_pool_mapper.debug("%s maps relocated task %s"
                               " taskWorkloadSize %d",
                               prolog(__FUNCTION__).c_str(),
                               taskDescription(task).c_str(),
                               taskWorkloadSize);
  }
  
  ProfilingRequest completionRequest;
  completionRequest.add_measurement<Realm::ProfilingMeasurements::OperationStatus>();
  output.task_prof_requests = completionRequest;
  
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
void TaskPoolMapper::select_task_options(const MapperContext    ctx,
                                         const Task&            task,
                                         TaskOptions&     output)
//--------------------------------------------------------------------------
{
  DefaultMapper::VariantInfo variantInfo =
    DefaultMapper::default_find_preferred_variant(task, ctx,
                                                  /*needs tight bound*/false,
                                                  /*cache result*/true,
                                                  Processor::NO_KIND); 

  bool selectThisTask = true;
  Processor initial_proc = local_proc;
  
  if(variantInfo.proc_kind == local_proc.kind()) {
    if(mapperCategory == TASK_POOL) {
      selectThisTask = taskWorkloadSize < MIN_TASKS_PER_PROCESSOR
      || !isAnalysisTask(task);
    }
  } else {
    switch(variantInfo.proc_kind) {
      case LOC_PROC:
        initial_proc = nearestLegionCPUProc;
        break;
      case IO_PROC:
        initial_proc = nearestIOProc;
        break;
      case PY_PROC:
        initial_proc = nearestTaskPoolProc;
        break;
      default: assert(false);
    }
  }

  if(selectThisTask) {
    log_task_pool_mapper.debug("%s %s on %s",
                               prolog(__FUNCTION__).c_str(),
                               taskDescription(task).c_str(),
                               processorKindString(initial_proc.kind()));
    output.initial_proc = initial_proc;
    output.inline_task = false;
    output.stealable = false;
    output.map_locally = false;
  } else {
    log_task_pool_mapper.debug("%s skip task %s",
                               prolog(__FUNCTION__).c_str(),
                               taskDescription(task).c_str());
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
void TaskPoolMapper::premap_task(const MapperContext      ctx,
                                 const Task&              task,
                                 const PremapTaskInput&   input,
                                 PremapTaskOutput&        output)
//------------------------------------------------------------------------
{
  log_task_pool_mapper.debug("%s premap_task %s",
                             prolog(__FUNCTION__).c_str(),
                             taskDescription(task).c_str());
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
    TaskPoolMapper* mapper = new TaskPoolMapper(runtime->get_mapper_runtime(),
                                                machine, *it, "task_pool_mapper",
                                                procs_list,
                                                sysmems_list,
                                                sysmem_local_procs,
                                                proc_sysmems,
                                                proc_regmems);
    runtime->replace_default_mapper(mapper, *it);
  }
}

void register_task_pool_mapper()
{
  HighLevelRuntime::add_registration_callback(create_mappers);
}

