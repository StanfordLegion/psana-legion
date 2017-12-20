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

#include "lifeline_mapper.h"

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

static const int TASKS_PER_STEALABLE_SLICE = 1;
static const int MIN_TASKS_PER_PROCESSOR = 2;

typedef enum {
  STEAL_REQUEST = 1,
  STEAL_ACK,
  STEAL_NACK,
} MessageType;

typedef unsigned long long UniqueId;

typedef struct {
  UniqueId id;
  Processor victimProc;
  Processor thiefProc;
  unsigned numTasks;
} Request;

static LegionRuntime::Logger::Category log_lifeline_mapper("lifeline_mapper");


///
/// Mapper
///



class LifelineMapper : public DefaultMapper
{
public:
  LifelineMapper(MapperRuntime *rt,
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
  std::vector<Processor> lifeline_neighbor_procs;
  std::vector<Processor> steal_target_procs;
  Processor nearestLOCProc;
  Processor nearestIOProc;
  Processor nearestPYProc;
  std::random_device rd;     // only used once to initialise (seed) engine
  std::mt19937 rng;
  std::uniform_int_distribution<int> uni;
  std::map<std::pair<LogicalRegion,Memory>,PhysicalInstance> local_instances;
  typedef long long Timestamp;
  int taskWorkloadSize;
  MapperRuntime *runtime;
  MapperEvent defer_select_tasks_to_map;
  UniqueId uniqueId();
  
  std::set<const Task*> worker_ready_queue;
  typedef std::vector<std::pair<Request, std::vector<const Task*> > > SendQueue;
  SendQueue send_queue;
  unsigned numUniqueIds;
  bool stealRequestOutstanding;
  
  Timestamp timeNow() const;
  void establishLifelines();
  std::string taskDescription(const Legion::Task& task);
  std::string prolog(const char* function) const;
  char* processorKindString(unsigned kind) const;
  bool isAnalysisTask(const Legion::Task& task);
  void slice_task(const MapperContext      ctx,
                  const Task&              task,
                  const SliceTaskInput&    input,
                  SliceTaskOutput&   output);
  void decompose_points(const Rect<1, coord_t> &point_rect,
                        const std::vector<Processor> &targets,
                        const Point<1, coord_t> &num_blocks,
                        std::vector<TaskSlice> &slices);
  bool filterInputReadyTasks(const SelectMappingInput&    input,
                             SelectMappingOutput&   output);
  bool sendSatisfiedTasks(const MapperContext          ctx,
                          SelectMappingOutput&   output);
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
  void triggerSelectTasksToMap(const MapperContext ctx);
  void handleStealRequest(const MapperContext ctx,
                          const MapperMessage& message);
  void handleStealAck(const MapperContext ctx,
                      const MapperMessage& message);
  void handleStealNack(const MapperContext ctx,
                       const MapperMessage& message);
  virtual void handle_message(const MapperContext           ctx,
                              const MapperMessage&          message);
  
  const char* get_mapper_name(void) const { return "lifeline_mapper"; }
  MapperSyncModel get_mapper_sync_model(void) const {
    return SERIALIZED_REENTRANT_MAPPER_MODEL;
  }
};

//--------------------------------------------------------------------------
static std::string describeProcId(long long procId)
//--------------------------------------------------------------------------
{
  char buffer[128];
  unsigned nodeId = procId >> 40;
  unsigned pId = procId & 0xffffffffff;
  sprintf(buffer, "node %x proc %x", nodeId, pId);
  return std::string(buffer);
}


//--------------------------------------------------------------------------
LifelineMapper::LifelineMapper(MapperRuntime *rt,
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
  log_lifeline_mapper.info("%s constructor", prolog(__FUNCTION__).c_str());
  establishLifelines();
  
  rng = std::mt19937(rd());    // random-number engine used (Mersenne-Twister in this case)
  uni = std::uniform_int_distribution<int>(0, (int)steal_target_procs.size() - 1); // guaranteed unbiased
  runtime = rt;
  numUniqueIds = 0;
  stealRequestOutstanding = false;
  taskWorkloadSize = 0;
  log_lifeline_mapper.info("%lld # %s taskWorkloadSize %d %s",
                           timeNow(), describeProcId(local_proc.id).c_str(),
                           taskWorkloadSize,
                           processorKindString(local_proc.kind()));
}



//--------------------------------------------------------------------------
std::string LifelineMapper::prolog(const char* function) const
//--------------------------------------------------------------------------
{
  char buffer[512];
  assert(strlen(function) < sizeof(buffer) - 64);
  sprintf(buffer, "%lld %s(%s): %s",
          timeNow(), describeProcId(local_proc.id).c_str(),
          processorKindString(local_proc.kind()),
          function);
  return std::string(buffer);
}


//--------------------------------------------------------------------------
UniqueId LifelineMapper::uniqueId()
//--------------------------------------------------------------------------
{
  UniqueId nodeId = local_proc.id >> 40;
  UniqueId pId = local_proc.id & 0xffffffffff;
  unsigned maxNodeId = 128 * 1024;
  unsigned maxProcId = 128;
  UniqueId result = numUniqueIds++ * (maxNodeId * maxProcId) + (nodeId * pId);
  return result;
}

//--------------------------------------------------------------------------
void LifelineMapper::maybeSendStealRequest(MapperContext ctx)
//--------------------------------------------------------------------------
{
  if(taskWorkloadSize < MIN_TASKS_PER_PROCESSOR && !stealRequestOutstanding) {
    unsigned numTasks = MIN_TASKS_PER_PROCESSOR - taskWorkloadSize;
    Processor target = steal_target_procs[uni(rng)];
    while(target.id == local_proc.id) {
      target = steal_target_procs[uni(rng)];
    }
    Request r = { uniqueId(), target, local_proc, numTasks };
    log_lifeline_mapper.debug("%s send STEAL_REQUEST id %lld numTasks %d to %s",
                              prolog(__FUNCTION__).c_str(),
                              r.id, numTasks, describeProcId(target.id).c_str());
    stealRequestOutstanding = true;
    runtime->send_message(ctx, target, &r, sizeof(r), STEAL_REQUEST);
  } else {
    if(taskWorkloadSize < MIN_TASKS_PER_PROCESSOR && stealRequestOutstanding) {
      log_lifeline_mapper.debug("%s cannot send because stealRequestOutstanding",
                                prolog(__FUNCTION__).c_str());
    }
  }
}


//--------------------------------------------------------------------------
void LifelineMapper::triggerSelectTasksToMap(const MapperContext ctx)
//--------------------------------------------------------------------------
{
  if(defer_select_tasks_to_map.exists()){
    MapperEvent temp_event = defer_select_tasks_to_map;
    defer_select_tasks_to_map = MapperEvent();
    runtime->trigger_mapper_event(ctx, temp_event);
  }
}


//--------------------------------------------------------------------------
void LifelineMapper::handleStealRequest(const MapperContext ctx,
                                        const MapperMessage& message)
//--------------------------------------------------------------------------
{
  
}

//--------------------------------------------------------------------------
void LifelineMapper::handleStealAck(const MapperContext ctx,
                                    const MapperMessage& message)
//--------------------------------------------------------------------------
{
  
}

//--------------------------------------------------------------------------
void LifelineMapper::handleStealNack(const MapperContext ctx,
                                     const MapperMessage& message)
//--------------------------------------------------------------------------
{
  
}

//--------------------------------------------------------------------------
void LifelineMapper::handle_message(const MapperContext ctx,
                                    const MapperMessage& message)
//--------------------------------------------------------------------------
{
  switch(message.kind) {
    case STEAL_REQUEST:
      handleStealRequest(ctx, message);
      break;
    case STEAL_ACK:
      handleStealAck(ctx, message);
      break;
    case STEAL_NACK:
      handleStealNack(ctx, message);
      break;
    default: assert(false);
  }
}

//--------------------------------------------------------------------------
LifelineMapper::Timestamp LifelineMapper::timeNow() const
//--------------------------------------------------------------------------
{
  return Realm::Clock::current_time_in_nanoseconds();
}


//--------------------------------------------------------------------------
void LifelineMapper::establishLifelines()
//--------------------------------------------------------------------------
{
  unsigned localProcId;
  bool needLOCProc = false;
  bool needIOProc = false;
  bool needPYProc = false;
  Processor recentLOCProc;
  Processor recentIOProc;
  Processor recentPYProc;
  
  for(std::map<Processor, Memory>::iterator it = proc_sysmems.begin();
      it != proc_sysmems.end(); it++) {
    Processor processor = it->first;

    switch(processor.kind()) {
      case LOC_PROC:
        if(needLOCProc) {
          nearestLOCProc = processor;
        } else {
          recentLOCProc = processor;
        }
        break;
      case IO_PROC:
        if(needIOProc) {
          nearestIOProc = processor;
        } else {
          recentIOProc = processor;
        }
        break;
      case PY_PROC:
        if(needPYProc) {
          nearestPYProc = processor;
        } else {
          recentPYProc = processor;
        }
        break;
      default: assert(false);
    }
    
    if(processor.kind() == local_proc.kind()) {
      if(processor == local_proc) {
        localProcId = (unsigned)steal_target_procs.size();
        if(recentLOCProc.exists()) {
          nearestLOCProc = recentLOCProc;
        } else {
          needLOCProc = true;
        }
        if(recentIOProc.exists()) {
          nearestIOProc = recentIOProc;
        } else {
          needIOProc = true;
        }
        if(recentPYProc.exists()) {
          nearestPYProc = recentPYProc;
       } else {
          needPYProc = true;
        }
      }
      steal_target_procs.push_back(processor);
    }
  }
  
  assert(nearestLOCProc.exists());
  assert(nearestIOProc.exists());
  assert(nearestPYProc.exists());

  if(steal_target_procs.size() > 1) {
    unsigned maxProcId = pow(2.0, (unsigned)log2(steal_target_procs.size() - 1) + 1);
    unsigned numIdBits = (unsigned)log2(maxProcId);
    
    for(unsigned bit = 0; bit < numIdBits; bit++) {
      unsigned mask = 1 << bit;
      unsigned sourceBit = localProcId & mask;
      unsigned targetProcId = (localProcId & ~mask) | (~sourceBit & (maxProcId - 1));
      lifeline_neighbor_procs.push_back(steal_target_procs[targetProcId]);
      
      log_lifeline_mapper.debug("%s lifeline to %s",
                                prolog(__FUNCTION__).c_str(),
                                describeProcId(steal_target_procs[targetProcId].id).c_str());
    }
  }
}

//--------------------------------------------------------------------------
std::string LifelineMapper::taskDescription(const Legion::Task& task)
//--------------------------------------------------------------------------
{
  char buffer[512];
  sprintf(buffer, "<%s:%llx>", task.get_task_name(), task.get_unique_id());
  return std::string(buffer);
}


//--------------------------------------------------------------------------
bool LifelineMapper::isAnalysisTask(const Legion::Task& task)
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
void LifelineMapper::decompose_points(const Rect<1, coord_t> &point_rect,
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
void LifelineMapper::slice_task(const MapperContext      ctx,
                                const Task&              task,
                                const SliceTaskInput&    input,
                                SliceTaskOutput&   output)
//--------------------------------------------------------------------------
{
  
  if(isAnalysisTask(task)){
    log_lifeline_mapper.debug("%s task %s target proc %s proc_kind %d",
                              prolog(__FUNCTION__).c_str(),
                              taskDescription(task).c_str(),
                              describeProcId(task.target_proc.id).c_str(),
                              task.target_proc.kind());
    assert(input.domain.get_dim() == 1);
    
    Rect<1, coord_t> point_rect = input.domain;
    Point<1, coord_t> num_blocks(point_rect.volume() / TASKS_PER_STEALABLE_SLICE);
    decompose_points(point_rect, lifeline_neighbor_procs, num_blocks, output.slices);
    
  } else {
    log_lifeline_mapper.debug("%s pass %s to default mapper",
                              prolog(__FUNCTION__).c_str(),
                              taskDescription(task).c_str());
    this->DefaultMapper::slice_task(ctx, task, input, output);
  }
}

#if 0
//--------------------------------------------------------------------------
void LifelineMapper::handleStealRequest(const MapperContext          ctx,
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
      log_lifeline_mapper.debug("%s send "
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
#endif

//--------------------------------------------------------------------------
char* LifelineMapper::processorKindString(unsigned kind) const
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
      log_lifeline_mapper.debug("%s processor kind %d",
                                prolog(__FUNCTION__).c_str(), kind);
      assert(false);
  }
}

//--------------------------------------------------------------------------
void LifelineMapper::select_tasks_to_map(const MapperContext          ctx,
                                         const SelectMappingInput&    input,
                                         SelectMappingOutput&   output)
//--------------------------------------------------------------------------
{
  
#if 0
  
#if VERBOSE_DEBUG
  for(std::list<const Task*>::const_iterator it = input.ready_tasks.begin();
      it != input.ready_tasks.end(); it++) {
    log_lifeline_mapper.debug("%s input.ready_tasks %s",
                              prolog(__FUNCTION__).c_str(),
                              taskDescription(**it).c_str());
  }
#endif
  
  if(defer_select_tasks_to_map.exists()) {
    triggerSelectTasksToMap(ctx);
  }
  
  if(mapperCategory == TASK_POOL) {
    
    log_lifeline_mapper.debug("%s readyTasks size %ld",
                              prolog(__FUNCTION__).c_str(),
                              input.ready_tasks.size());
    
    bool mapped = filterInputReadyTasks(input, output);
    
    
    // Notify any failed requests that we have work
    if (!worker_ready_queue.empty())
      wakeUpWorkers(ctx, (unsigned)worker_ready_queue.size());
    
    mapped |= sendSatisfiedTasks(ctx, output);
    assert(send_queue.empty());
    
    if (!mapped && !input.ready_tasks.empty()) {
      log_lifeline_mapper.debug("%s trigger subsequent invocation",
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
        log_lifeline_mapper.debug("%s %s selects %s",
                                  prolog(__FUNCTION__).c_str(),
                                  processorKindString(local_proc.kind()),
                                  taskDescription(*task).c_str());
        output.map_tasks.insert(task);
      } else {
        log_lifeline_mapper.debug("%s %s relocates %s to %s",
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
    log_lifeline_mapper.debug("%s output.map_tasks %s",
                              prolog(__FUNCTION__).c_str(),
                              taskDescription(*task).c_str());
  }
  for(std::map<const Task*, Processor>::const_iterator relIt = output.relocate_tasks.begin();
      relIt != output.relocate_tasks.end(); ++relIt) {
    const Task* task = relIt->first;
    Processor p = relIt->second;
    log_lifeline_mapper.debug("%s output.relocate_Tasks %s %s",
                              prolog(__FUNCTION__).c_str(),
                              taskDescription(*task).c_str(), (int)(p.id & 0xff));
  }
#endif
  
#endif
  
}




//--------------------------------------------------------------------------
void LifelineMapper::select_steal_targets(const MapperContext         ctx,
                                          const SelectStealingInput&  input,
                                          SelectStealingOutput& output)
//--------------------------------------------------------------------------
{
  maybeSendStealRequest(ctx);
}


//------------------------------------------------------------------------------
Memory LifelineMapper::get_associated_sysmem(Processor proc)
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
void LifelineMapper::map_task_array(const MapperContext ctx,
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
    log_lifeline_mapper.error("TASK_POOL mapper failed to allocate instance");
    assert(false);
  }
  instances.push_back(result);
  local_instances[key] = result;
}

//--------------------------------------------------------------------------
void LifelineMapper::report_profiling(const MapperContext      ctx,
                                      const Task&              task,
                                      const TaskProfilingInfo& input)
//--------------------------------------------------------------------------
{
  // task completion request
  taskWorkloadSize--;
  log_lifeline_mapper.info("%s report_profiling # %s taskWorkloadSize %d",
                           prolog(__FUNCTION__).c_str(),
                           taskDescription(task).c_str(),
                           taskWorkloadSize);
  maybeSendStealRequest(ctx);
}

//--------------------------------------------------------------------------
void LifelineMapper::map_task(const MapperContext      ctx,
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
    log_lifeline_mapper.debug("%s maps self task %s"
                              " taskWorkloadSize %d",
                              prolog(__FUNCTION__).c_str(),
                              taskDescription(task).c_str(),
                              taskWorkloadSize);
  } else {
    taskWorkloadSize++;
    log_lifeline_mapper.debug("%s maps relocated task %s"
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
void LifelineMapper::select_task_options(const MapperContext    ctx,
                                         const Task&            task,
                                         TaskOptions&     output)
//--------------------------------------------------------------------------
{
  log_lifeline_mapper.debug("%s task %s",
                            prolog(__FUNCTION__).c_str(),
                            taskDescription(task).c_str());
  
  DefaultMapper::VariantInfo variantInfo =
  DefaultMapper::default_find_preferred_variant(task, ctx,
                                                /*needs tight bound*/false,
                                                /*cache result*/true,
                                                Processor::NO_KIND);
  
  Processor initial_proc = local_proc;
  
  if(variantInfo.proc_kind != local_proc.kind()) {
    switch(variantInfo.proc_kind) {
      case LOC_PROC:
        initial_proc = nearestLOCProc;
        break;
      case IO_PROC:
        initial_proc = nearestIOProc;
        break;
      case PY_PROC:
        initial_proc = nearestPYProc;
        break;
      default: assert(false);
    }
  }
  
  log_lifeline_mapper.debug("%s %s on %s",
                            prolog(__FUNCTION__).c_str(),
                            taskDescription(task).c_str(),
                            processorKindString(initial_proc.kind()));
  output.initial_proc = initial_proc;
  output.inline_task = false;
  output.stealable = false;
  output.map_locally = false;
  
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
void LifelineMapper::premap_task(const MapperContext      ctx,
                                 const Task&              task,
                                 const PremapTaskInput&   input,
                                 PremapTaskOutput&        output)
//------------------------------------------------------------------------
{
  log_lifeline_mapper.debug("%s premap_task %s",
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
    LifelineMapper* mapper = new LifelineMapper(runtime->get_mapper_runtime(),
                                                machine, *it, "lifeline_mapper",
                                                procs_list,
                                                sysmems_list,
                                                sysmem_local_procs,
                                                proc_sysmems,
                                                proc_regmems);
    runtime->replace_default_mapper(mapper, *it);
  }
}

void register_lifeline_mapper()
{
  HighLevelRuntime::add_registration_callback(create_mappers);
}

