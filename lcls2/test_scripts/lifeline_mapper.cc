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

#include "lifeline_mapper.h"

#include <map>
#include <random>
#include <time.h>
#include <vector>
#include <deque>
#include <sysexits.h>
#include <unistd.h>



#include "default_mapper.h"


#undef assert
#define assert(condition) {if(!(condition)) { log_lifeline_mapper.fatal("%s assert %s", prolog(__FUNCTION__, __LINE__).c_str(), #condition); exit(EX_SOFTWARE); }}

using namespace Legion;
using namespace Legion::Mapping;


static const char* ANALYSIS_TASK_NAMES[] = {
  "sum_task",
  "psana_legion.analyze_single"
};

static const int TASKS_PER_STEALABLE_SLICE = 1;
static int MIN_RUNNING_ANALYSIS_TASKS = 4;
static int MIN_RUNNING_PY_TASKS = 6;
static const int MAX_FAILED_STEALS = 5;

typedef enum {
  STEAL_REQUEST = 1,
  STEAL_ACK,
  STEAL_NACK,
  ACTIVATE_LIFELINE,
  WITHDRAW_LIFELINE,
  LIFELINE_WAKEUP,
  RELOCATE_TASK_INFO,
} MessageType;


typedef struct {
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
  std::vector<Processor>& procs_list;
  std::vector<Memory>& sysmems_list;
  //std::map<Memory, std::vector<Processor> >& sysmem_local_procs;
  std::map<Processor, Memory>& proc_sysmems;
  //  std::map<Processor, Memory>& proc_regmems;
  std::vector<Processor> lifeline_neighbor_procs;
  std::vector<Processor> steal_target_procs;
  std::vector<Processor> active_lifelines;
  std::vector<Processor> LOCProcs;
  std::vector<Processor> TOCProcs;
  Processor nearestLOCProc;
  Processor nearestTOCProc;
  Processor nearestIOProc;
  Processor nearestPYProc;
  std::vector<Processor> neighborLOCProcs;
  std::vector<Processor> neighborTOCProcs;
  std::vector<Processor> neighborIOProcs;
  std::vector<Processor> neighborPYProcs;
  std::random_device rd;     // only used once to initialise (seed) engine
  std::mt19937 rng;
  std::uniform_int_distribution<int> uni;
  std::uniform_int_distribution<int> uniLOC;
  std::uniform_int_distribution<int> uniTOC;
  std::map<std::pair<LogicalRegion,Memory>,PhysicalInstance> local_instances;
  typedef long long Timestamp;
  
  // these are the counters that track workload
  int locallyStartedTaskCount; // tasks that start running here
  int locallyEndedTaskCount; // tasks that report completion here
  int stolenAwayTaskCount; // tasks that are stolen away due to receiving a steal request from another processor
  int promisedFromStealsTaskCount; // tasks that are promised here by another processor because we sent a steal request
  int promisedRelocatedTaskCount; // tasks that are relocated here from another processor
  int sliceTaskCount; // slice tasks seen here in slice_task
  int slicedPointTaskCount; // point tasks generated here from slice tasks
  int selfGeneratedTaskCount; // tasks for processor of our kind() seen here in select_task_options
  int mappedRelocatedTaskCount; // tasks that were sent here from another processor and mapped
  int mappedSelfGeneratedTaskCount; // tasks that were generated here and mapped
  unsigned taskSerialId;
  
  MapperRuntime *runtime;
  MapperEvent defer_select_tasks_to_map;
  std::set<const Task*> worker_ready_queue;
  std::set<const Task*> relocated_tasks;
  std::set<const Task*> tasks_to_map_locally;
  typedef std::vector<const Task*> TaskVector;
  typedef std::map<Processor, TaskVector> SendQueue;
  SendQueue send_queue;
  unsigned numUniqueIds;
  bool stealRequestOutstanding;
  //  std::deque<Request> unresolved_requests;
  unsigned numFailedSteals;
  bool quiesced;
  
  int locallyRunningTaskCount() const;
  int totalPendingWorkload() const;
  std::string workloadState() const;
  Timestamp timeNow() const;
  void getStealAndNearestProcs(unsigned& localProcIndex);
  void getNearestProcs(Processor processor, bool sawLocalProc);
  void getStealProcs(Processor processor, bool& sawLocalProc, unsigned& localProcIndex);
  void identifyRelatedProcs();
  Processor nearestProcessor(unsigned kind);
  Processor randomProcessor(unsigned kind);
  std::string taskDescription(const Legion::Task& task);
  std::string prolog(const char* function, int line) const;
  char* processorKindString(unsigned kind) const;
  bool isAnalysisTask(const Legion::Task& task);
  int minRunningTasks() const;
  bool isNodeZero() const;
  static unsigned nodeId(long long procId);
  static std::string describeProcId(long long procId);

  void configure_context(const MapperContext         ctx,
                         const Task&                 task,
                         ContextConfigOutput&  output);
  void slice_task(const MapperContext      ctx,
                  const Task&              task,
                  const SliceTaskInput&    input,
                  SliceTaskOutput&   output);
  void custom_slice_task(const Task &task,
                         const std::vector<Processor> &local,
                         const std::vector<Processor> &remote,
                         const SliceTaskInput &input,
                         SliceTaskOutput &output,
                 std::map<Domain,std::vector<TaskSlice> > &cached_slices) const;
  void decompose_points(const Rect<1, coord_t> &point_rect,
                        const Point<1, coord_t> &num_blocks,
                        std::vector<TaskSlice> &slices);
  bool filterInputReadyTasks(const SelectMappingInput&    input,
                             SelectMappingOutput&   output);
  bool sendSatisfiedTasks(const MapperContext          ctx,
                          SelectMappingOutput&   output);
  void giveLifeThroughLifelines(const MapperContext          ctx);
  void processNewReadyTasks(const SelectMappingInput&    input,
                            SelectMappingOutput&   output,
                            bool& mappedOrRelocated,
                            bool& sawWorkerReadyTasks);
  void select_tasks_to_map(const MapperContext          ctx,
                           const SelectMappingInput&    input,
                           SelectMappingOutput&   output);
  void select_steal_targets(const MapperContext         ctx,
                            const SelectStealingInput&  input,
                            SelectStealingOutput& output);
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
  bool alreadyQueued(const Task* task);
  bool send_stolen_tasks(MapperContext ctx,
                         const SelectMappingInput&    input,
                         SelectMappingOutput&   output);
  void assignTaskId(const MapperContext    ctx,
                    const Task& task);
  void select_task_options(const MapperContext    ctx,
                           const Task&            task,
                           TaskOptions&     output);
  void premap_task(const MapperContext      ctx,
                   const Task&              task,
                   const PremapTaskInput&   input,
                   PremapTaskOutput&        output);
  void sendStealRequest(MapperContext ctx, Processor target);
  bool maybeGetLocalTasks(MapperContext ctx);
  Processor stealTargetProcessor();
  void stealTasks(MapperContext ctx, Processor target);
  void maybeGetMoreTasks(MapperContext ctx, Processor target=Processor::NO_PROC);
  void triggerSelectTasksToMap(const MapperContext ctx);
  void handleRelocateTaskInfo(const MapperContext ctx,
                              const MapperMessage& message);
  void handleOneStealRequest(const MapperContext ctx, Request r,
                             bool reactivating, bool& stolen);
  void handleStealRequest(const MapperContext ctx,
                          const MapperMessage& message);
  void handleStealAck(const MapperContext ctx,
                      const MapperMessage& message);
  void withdrawActiveLifelines(const MapperContext ctx);
  void quiesce(const MapperContext ctx);
  void handleStealNack(const MapperContext ctx,
                       const MapperMessage& message);
  void handleWithdrawLifeline(const MapperContext ctx,
                              const MapperMessage& message);
  void handleLifelineWakeup(const MapperContext ctx,
                            const MapperMessage& message);
  void handleActivateLifeline(const MapperContext ctx,
                              const MapperMessage& message);
  virtual void handle_message(const MapperContext           ctx,
                              const MapperMessage&          message);
  
  const char* get_mapper_name(void) const { return "lifeline_mapper"; }
  MapperSyncModel get_mapper_sync_model(void) const {
    return SERIALIZED_NON_REENTRANT_MAPPER_MODEL;
  }
};

//--------------------------------------------------------------------------
std::string LifelineMapper::describeProcId(long long procId)
//--------------------------------------------------------------------------
{
  char buffer[128];
  unsigned pId = procId & 0xffffffffff;
  sprintf(buffer, "node %x proc %x", nodeId(procId), pId);
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
procs_list(*_procs_list),
sysmems_list(*_sysmems_list),
//sysmem_local_procs(*_sysmem_local_procs),
proc_sysmems(*_proc_sysmems)
// proc_regmems(*_proc_regmems)
//--------------------------------------------------------------------------
{
  log_lifeline_mapper.debug("%s pid %d proc_sysmems.size %ld",
                             prolog(__FUNCTION__, __LINE__).c_str(),
                             getpid(),
                             proc_sysmems.size());

  identifyRelatedProcs();
  
  rng = std::mt19937(rd());    // random-number engine used (Mersenne-Twister in this case)
  uni = std::uniform_int_distribution<int>(0, (int)steal_target_procs.size() - 1); // guaranteed unbiased
  uniLOC = std::uniform_int_distribution<int>(0, (int)LOCProcs.size() - 1);
  uniTOC = std::uniform_int_distribution<int>(0, (int)TOCProcs.size() - 1);
  runtime = rt;
  numUniqueIds = 0;
  stealRequestOutstanding = false;
  sliceTaskCount = 0;
  locallyStartedTaskCount = 0;
  locallyEndedTaskCount = 0;
  stolenAwayTaskCount = 0;
  promisedFromStealsTaskCount = 0;
  promisedRelocatedTaskCount = 0;
  slicedPointTaskCount = 0;
  selfGeneratedTaskCount = 0;
  mappedRelocatedTaskCount = 0;
  mappedSelfGeneratedTaskCount = 0;
  numFailedSteals = 0;
  taskSerialId = 0;
  quiesced = false;
  
  if(const char* env_p = std::getenv("PSANA_LEGION_MIN_RUNNING_PY_TASKS")) {
    sscanf("%d", env_p, &MIN_RUNNING_PY_TASKS);
  }
  if(const char* env_p = std::getenv("PSANA_LEGION_MIN_RUNNING_ANALYSIS_TASKS")) {
    sscanf("%d", env_p, &MIN_RUNNING_ANALYSIS_TASKS);
  }
}


//--------------------------------------------------------------------------
unsigned LifelineMapper::nodeId(long long procId)
//--------------------------------------------------------------------------
{
  unsigned nodeId = (procId >> 40) & 0xffff;
  return nodeId;
}


//--------------------------------------------------------------------------
void LifelineMapper::getNearestProcs(Processor processor, bool sawLocalProc)
//--------------------------------------------------------------------------
{
  
  if(nodeId(processor.id) > 0 || total_nodes == 1) {
    
    log_lifeline_mapper.debug("%s nodeId %d total nodes %d nearest proc %s", prolog(__FUNCTION__, __LINE__).c_str(), nodeId(processor.id), total_nodes, describeProcId(processor.id).c_str());
    
    switch(processor.kind()) {
      case LOC_PROC:
        LOCProcs.push_back(processor);
        if(!nearestLOCProc.exists() || !sawLocalProc) {
          nearestLOCProc = processor;
        }
        break;
      case TOC_PROC:
        TOCProcs.push_back(processor);
        if(!nearestTOCProc.exists() || !sawLocalProc) {
          nearestTOCProc = processor;
        }
        break;
      case IO_PROC:
        if(!nearestIOProc.exists() || !sawLocalProc) {
          nearestIOProc = processor;
        }
        break;
      case PY_PROC:
        if(!nearestPYProc.exists() || !sawLocalProc) {
          nearestPYProc = processor;
        }
        break;
      default: assert(false);
    }
  }

}


//--------------------------------------------------------------------------
void LifelineMapper::getStealProcs(Processor processor, bool& sawLocalProc, unsigned& localProcIndex)
//--------------------------------------------------------------------------
{
  if(!isNodeZero()) {
    
    bool isStealTarget = true;
    // python procs can steal from each other
    // LOC and TOC procs can steal from each other
    if(local_proc.kind() == Processor::PY_PROC) {
      isStealTarget = processor.kind() == local_proc.kind();
    } else if(local_proc.kind() == Processor::LOC_PROC) {
      isStealTarget = processor.kind() == Processor::TOC_PROC;
    } else if(local_proc.kind() == Processor::TOC_PROC) {
      isStealTarget = processor.kind() == Processor::LOC_PROC;
    }
    
    // when running multi-node ignore node zero for stealing purposes
    if(!isNodeZero() && nodeId(processor.id) == 0) {
      isStealTarget = false;
    }
    
    if(isStealTarget) {
      if(processor == local_proc) {
        localProcIndex = (unsigned)steal_target_procs.size();
        sawLocalProc = true;
      }
      steal_target_procs.push_back(processor);
    }
  }
}

//--------------------------------------------------------------------------
void LifelineMapper::getStealAndNearestProcs(unsigned& localProcIndex)
//--------------------------------------------------------------------------
{
  nearestLOCProc = Processor::NO_PROC;
  nearestIOProc = Processor::NO_PROC;
  nearestPYProc = Processor::NO_PROC;
  bool sawLocalProc = false;
  
  log_lifeline_mapper.debug("%s procs_list.size %ld",
                            prolog(__FUNCTION__, __LINE__).c_str(),
                            procs_list.size());
  
  for(std::vector<Processor>::iterator it = procs_list.begin();
      it != procs_list.end(); it++) {
    Processor processor = *it;
    getNearestProcs(processor, sawLocalProc);
    getStealProcs(processor, sawLocalProc, localProcIndex);
  }
  
  for(std::vector<Processor>::iterator it = steal_target_procs.begin();
      it != steal_target_procs.end(); ++it) {
    log_lifeline_mapper.debug("%s steal target %s",
                              prolog(__FUNCTION__, __LINE__).c_str(),
                              describeProcId(it->id).c_str());
  }
}

//--------------------------------------------------------------------------
void LifelineMapper::identifyRelatedProcs()
//--------------------------------------------------------------------------
{
  unsigned localProcIndex;
  getStealAndNearestProcs(localProcIndex);
  
  if(steal_target_procs.size() == 1) {
    lifeline_neighbor_procs.push_back(steal_target_procs[0]);
    log_lifeline_mapper.info("%s lifeline to %s",
                             prolog(__FUNCTION__, __LINE__).c_str(),
                             describeProcId(steal_target_procs[0].id).c_str());
  } else if(steal_target_procs.size() > 1) {
    unsigned maxProcId = pow(2.0, (unsigned)log2(steal_target_procs.size() - 1) + 1);
    unsigned numIdBits = (unsigned)log2(maxProcId);
    
    log_lifeline_mapper.debug("%s maxProcId %u numIdBits %u steal_targets.size %ld",
                              prolog(__FUNCTION__, __LINE__).c_str(),
                              maxProcId, numIdBits, steal_target_procs.size());
    
    for(unsigned bit = 0; bit < numIdBits; bit++) {
      unsigned mask = 1 << bit;
      unsigned sourceBit = localProcIndex & mask;
      unsigned modifiedBit = (!(sourceBit >> bit)) << bit;
      unsigned localProcIndexNoBit = (localProcIndex & ~mask) & (maxProcId - 1);
      unsigned targetProcIndex = localProcIndexNoBit | modifiedBit;
      
      if(targetProcIndex < steal_target_procs.size()) {
        lifeline_neighbor_procs.push_back(steal_target_procs[targetProcIndex]);
        
        log_lifeline_mapper.info("%s lifeline to %s",
                                 prolog(__FUNCTION__, __LINE__).c_str(),
                                 describeProcId(steal_target_procs[targetProcIndex].id).c_str());
      }
    }
  }
}



//--------------------------------------------------------------------------
void LifelineMapper::configure_context(const MapperContext         ctx,
                                       const Task&                 task,
                                       ContextConfigOutput&  output)
//--------------------------------------------------------------------------
{
  
}

//--------------------------------------------------------------------------
int LifelineMapper::totalPendingWorkload() const
//--------------------------------------------------------------------------
{
  int result = promisedFromStealsTaskCount + promisedRelocatedTaskCount + slicedPointTaskCount
  + selfGeneratedTaskCount - sliceTaskCount - stolenAwayTaskCount - locallyEndedTaskCount;
  log_lifeline_mapper.debug("%s = %d, promisedSteal %d promisedRelocated %d slicedPoint %d selfGenerated %d - slice %d stolen %d locallyEnded %d",
                            prolog(__FUNCTION__, __LINE__).c_str(),
                            result,
                            promisedFromStealsTaskCount,
                            promisedRelocatedTaskCount,
                            slicedPointTaskCount,
                            selfGeneratedTaskCount,
                            sliceTaskCount,
                            stolenAwayTaskCount,
                            locallyEndedTaskCount);
  return result;
}



//--------------------------------------------------------------------------
int LifelineMapper::locallyRunningTaskCount() const
//--------------------------------------------------------------------------
{
  int result = locallyStartedTaskCount - locallyEndedTaskCount;
  return result;
}


//--------------------------------------------------------------------------
std::string LifelineMapper::workloadState() const
//--------------------------------------------------------------------------
{
  char buffer[256];
  sprintf(buffer, "locallyRunning %d, totalPending %d = promisedSteal %d promisedRelocated %d slicedPoint %d selfGenerated %d - slice %d stolen %d locallyEnded %d",
          locallyRunningTaskCount(),
          totalPendingWorkload(),
          promisedFromStealsTaskCount,
          promisedRelocatedTaskCount,
          slicedPointTaskCount,
          selfGeneratedTaskCount,
          sliceTaskCount,
          stolenAwayTaskCount,
          locallyEndedTaskCount);
  return std::string(buffer);
}

//--------------------------------------------------------------------------
std::string LifelineMapper::prolog(const char* function, int line) const
//--------------------------------------------------------------------------
{
  char buffer[512];
  assert(strlen(function) < sizeof(buffer) - 64);
  sprintf(buffer, "%lld %s(%s): %s(%d)",
          timeNow(), describeProcId(local_proc.id).c_str(),
          processorKindString(local_proc.kind()),
          function, line);
  return std::string(buffer);
}


//--------------------------------------------------------------------------
void LifelineMapper::sendStealRequest(MapperContext ctx, Processor target)
//--------------------------------------------------------------------------
{
  unsigned numTasks = minRunningTasks() - locallyRunningTaskCount();
  assert(numTasks > 0);
  assert(target != Processor::NO_PROC);
  Request r = { target, local_proc, numTasks };
  log_lifeline_mapper.debug("%s send STEAL_REQUEST numTasks %d to %s",
                            prolog(__FUNCTION__, __LINE__).c_str(),
                            numTasks, describeProcId(target.id).c_str());
  stealRequestOutstanding = true;
  runtime->send_message(ctx, target, &r, sizeof(r), STEAL_REQUEST);
  
}

//--------------------------------------------------------------------------
bool LifelineMapper::maybeGetLocalTasks(MapperContext ctx)
//--------------------------------------------------------------------------
{
  if(locallyRunningTaskCount() < minRunningTasks() && worker_ready_queue.size() > 0
) {
    int numTasks = minRunningTasks() - locallyRunningTaskCount();
    
    for(std::set<const Task*>::iterator it = worker_ready_queue.begin();
        it != worker_ready_queue.end() && numTasks > 0; ) {
      const Task* task = *it;
      tasks_to_map_locally.insert(task);
      it = worker_ready_queue.erase(it);
      numTasks--;
      if(isAnalysisTask(*task)) {
        locallyStartedTaskCount++;
      }
      log_lifeline_mapper.debug("%s task %s should map locally, tasks_to_map_locally.size %ld",
                                prolog(__FUNCTION__, __LINE__).c_str(),
                                taskDescription(*task).c_str(),
                                tasks_to_map_locally.size());
    }
    triggerSelectTasksToMap(ctx);
    return true;
  } else {
    log_lifeline_mapper.debug("%s not getting local tasks because locallyRunningTaskCount %d too high or worker_ready_queue.size %ld is zero",
                              prolog(__FUNCTION__, __LINE__).c_str(), locallyRunningTaskCount(), worker_ready_queue.size());
  }
  return false;
}

//--------------------------------------------------------------------------
Processor LifelineMapper::stealTargetProcessor()
//--------------------------------------------------------------------------
{
  Processor target = steal_target_procs[uni(rng)];
  while(target.id == local_proc.id) {
    target = steal_target_procs[uni(rng)];
  }
  return target;
}


//--------------------------------------------------------------------------
void LifelineMapper::stealTasks(MapperContext ctx, Processor target)
//--------------------------------------------------------------------------
{
  log_lifeline_mapper.debug("%s target %s", prolog(__FUNCTION__, __LINE__).c_str(), describeProcId(target.id).c_str());
  if(local_proc.kind() == Processor::LOC_PROC || local_proc.kind() == Processor::TOC_PROC) {
    if (!stealRequestOutstanding) {
      if(target == Processor::NO_PROC) {
        target = stealTargetProcessor();
      }
      sendStealRequest(ctx, target);
    } else {
      log_lifeline_mapper.debug("%s not stealing because request outstanding",
                                prolog(__FUNCTION__, __LINE__).c_str());
    }
  }
}


//--------------------------------------------------------------------------
void LifelineMapper::maybeGetMoreTasks(MapperContext ctx, Processor target)
//--------------------------------------------------------------------------
{
  if(local_proc.kind() == Processor::LOC_PROC || local_proc.kind() == Processor::TOC_PROC || local_proc.kind() == Processor::PY_PROC) {
    if(!quiesced) {
      int notRunning = minRunningTasks() - locallyRunningTaskCount();
      bool wantMoreTasks = notRunning > 0 && !isNodeZero();
      bool havePendingTasks = totalPendingWorkload() >= notRunning;
      log_lifeline_mapper.debug("%s wantMore %d havePending %d stealOutstanding %d %s",
                                prolog(__FUNCTION__, __LINE__).c_str(),
                                wantMoreTasks, havePendingTasks, stealRequestOutstanding,
                                workloadState().c_str());
      if(wantMoreTasks) {
        if(!maybeGetLocalTasks(ctx)) {
          stealTasks(ctx, target);
        }
      }
    } else {
      log_lifeline_mapper.debug("%s not stealing because quiesced",
                                prolog(__FUNCTION__, __LINE__).c_str());
    }
  }
}




//--------------------------------------------------------------------------
void LifelineMapper::triggerSelectTasksToMap(const MapperContext ctx)
//--------------------------------------------------------------------------
{
  log_lifeline_mapper.debug("%s defer_select_tasks_to_map.exists %d",
                            prolog(__FUNCTION__, __LINE__).c_str(),
                            defer_select_tasks_to_map.exists());
  if(defer_select_tasks_to_map.exists()){
    MapperEvent temp_event = defer_select_tasks_to_map;
    defer_select_tasks_to_map = MapperEvent();
    runtime->trigger_mapper_event(ctx, temp_event);
  }
}


//--------------------------------------------------------------------------
void LifelineMapper::handleOneStealRequest(const MapperContext ctx,
                                           Request r,
                                           bool reactivating,
                                           bool& stolen)
//--------------------------------------------------------------------------
{
  log_lifeline_mapper.debug("%s victim %s thief %s, numTasks %d",
                            prolog(__FUNCTION__, __LINE__).c_str(),
                            describeProcId(r.victimProc.id).c_str(),
                            describeProcId(r.thiefProc.id).c_str(),
                            r.numTasks);
  
  assert(r.numTasks > 0);
  
  TaskVector tasks = send_queue[r.thiefProc];
  std::set<const Task*>::iterator it = worker_ready_queue.begin();
  
  unsigned numStolen = 0;
  while(r.numTasks > 0 && it != worker_ready_queue.end()) {
    const Task* task = *it;
    
    if(isAnalysisTask(*task)) {
      tasks.push_back(task);
      relocated_tasks.insert(task);
      r.numTasks--;
      numStolen++;
      stolenAwayTaskCount++;
      
      it = worker_ready_queue.erase(it);
      log_lifeline_mapper.debug("%s relocate task %s to %s %s",
                                prolog(__FUNCTION__, __LINE__).c_str(),
                                taskDescription(*task).c_str(),
                                describeProcId(r.thiefProc.id).c_str(),
                                workloadState().c_str());
    } else {
      it++;
    }
  }
  
  if(numStolen > 0) {
    send_queue[r.thiefProc] = tasks;
    stolen = true;
  } else {
    stolen = false;
    if(!reactivating) {
      log_lifeline_mapper.debug("%s send STEAL_NACK to %s",
                                prolog(__FUNCTION__, __LINE__).c_str(),
                                describeProcId(r.thiefProc.id).c_str());
      runtime->send_message(ctx, r.thiefProc, NULL, 0, STEAL_NACK);
    }
  }
}

//--------------------------------------------------------------------------
void LifelineMapper::handleStealRequest(const MapperContext ctx,
                                        const MapperMessage& message)
//--------------------------------------------------------------------------
{
  log_lifeline_mapper.debug("%s from %s worker_ready_queue.size %ld",
                            prolog(__FUNCTION__, __LINE__).c_str(),
                            describeProcId(message.sender.id).c_str(),
                            worker_ready_queue.size());
  if (worker_ready_queue.empty()) {
    Request r = *(Request*)message.message;
    log_lifeline_mapper.debug("%s send STEAL_NACK to %s",
                              prolog(__FUNCTION__, __LINE__).c_str(),
                              describeProcId(r.thiefProc.id).c_str());
    runtime->send_message(ctx, r.thiefProc, &r, sizeof(r), STEAL_NACK);
  } else {
    Request r = *(Request*)message.message;
    bool stolen;
    handleOneStealRequest(ctx, r, false, stolen);
  }
  triggerSelectTasksToMap(ctx);
}

//--------------------------------------------------------------------------
void LifelineMapper::handleStealAck(const MapperContext ctx,
                                    const MapperMessage& message)
//--------------------------------------------------------------------------
{
  Request r = *(Request*)message.message;
  promisedFromStealsTaskCount += r.numTasks;
  
  log_lifeline_mapper.debug("%s from %s numTasks %u %s",
                            prolog(__FUNCTION__, __LINE__).c_str(),
                            describeProcId(message.sender.id).c_str(),
                            r.numTasks, workloadState().c_str());
  if(quiesced) {
    withdrawActiveLifelines(ctx);
  }
  numFailedSteals = 0;
  stealRequestOutstanding = false;
}

//--------------------------------------------------------------------------
void LifelineMapper::handleRelocateTaskInfo(const MapperContext ctx,
                                            const MapperMessage& message)
//--------------------------------------------------------------------------
{
  Request r = *(Request*)message.message;
  promisedRelocatedTaskCount += r.numTasks;
  
  log_lifeline_mapper.debug("%s from %s numTasks %u %s",
                            prolog(__FUNCTION__, __LINE__).c_str(),
                            describeProcId(message.sender.id).c_str(),
                            r.numTasks, workloadState().c_str());
}

//--------------------------------------------------------------------------
void LifelineMapper::withdrawActiveLifelines(const MapperContext ctx)
//--------------------------------------------------------------------------
{
  assert(quiesced);
  for(std::vector<Processor>::iterator it = lifeline_neighbor_procs.begin();
      it != lifeline_neighbor_procs.end(); ++it) {
    Processor processor = *it;
    log_lifeline_mapper.debug("%s %s",
                              prolog(__FUNCTION__, __LINE__).c_str(),
                              describeProcId(processor.id).c_str());
  }
  for(std::vector<Processor>::iterator it = lifeline_neighbor_procs.begin();
      it != lifeline_neighbor_procs.end(); ++it) {
    Processor processor = *it;
    log_lifeline_mapper.debug("%s send WITHDRAW_LIFELINE to %s",
                              prolog(__FUNCTION__, __LINE__).c_str(),
                              describeProcId(processor.id).c_str());
    runtime->send_message(ctx, processor, NULL, 0, WITHDRAW_LIFELINE);
  }
  quiesced = false;
  numFailedSteals = 0;
  triggerSelectTasksToMap(ctx);
}

//--------------------------------------------------------------------------
void LifelineMapper::quiesce(const MapperContext ctx)
//--------------------------------------------------------------------------
{
  assert(!quiesced);
  log_lifeline_mapper.debug("%s", prolog(__FUNCTION__, __LINE__).c_str());
  for(std::vector<Processor>::iterator it = lifeline_neighbor_procs.begin();
      it != lifeline_neighbor_procs.end(); ++it) {
    Processor processor = *it;
    log_lifeline_mapper.debug("%s send ACTIVATE_LIFELINE to %s",
                              prolog(__FUNCTION__, __LINE__).c_str(),
                              describeProcId(processor.id).c_str());
    runtime->send_message(ctx, processor, NULL, 0, ACTIVATE_LIFELINE);
  }
  quiesced = true;
}

//--------------------------------------------------------------------------
void LifelineMapper::handleStealNack(const MapperContext ctx,
                                     const MapperMessage& message)
//--------------------------------------------------------------------------
{
  numFailedSteals++;
  stealRequestOutstanding = false;
  log_lifeline_mapper.debug("%s from %s num failed steals %d",
                            prolog(__FUNCTION__, __LINE__).c_str(),
                            describeProcId(message.sender.id).c_str(),
                            numFailedSteals);
  if(numFailedSteals >= MAX_FAILED_STEALS && locallyRunningTaskCount() < minRunningTasks()) {
    quiesce(ctx);
  } else {
    maybeGetMoreTasks(ctx);
  }
}

//--------------------------------------------------------------------------
void LifelineMapper::handleActivateLifeline(const MapperContext ctx,
                                            const MapperMessage& message)
//--------------------------------------------------------------------------
{
  log_lifeline_mapper.debug("%s from %s",
                            prolog(__FUNCTION__, __LINE__).c_str(),
                            describeProcId(message.sender.id).c_str());
  bool alreadyActive = false;
  for(std::vector<Processor>::iterator it = active_lifelines.begin();
      it != active_lifelines.end(); it++) {
    Processor sender = *it;
    if(sender.id == message.sender.id) alreadyActive = true;
  }
  if(!alreadyActive) {
    active_lifelines.push_back(message.sender);
  }
}


//--------------------------------------------------------------------------
void LifelineMapper::handleWithdrawLifeline(const MapperContext ctx,
                                            const MapperMessage& message)
//--------------------------------------------------------------------------
{
  log_lifeline_mapper.debug("%s",
                            prolog(__FUNCTION__, __LINE__).c_str());
  for(std::vector<Processor>::iterator it = active_lifelines.begin();
      it != active_lifelines.end(); ) {
    Processor processor = *it;
    if(processor.id == message.sender.id) {
      it = active_lifelines.erase(it);
      log_lifeline_mapper.debug("%s from %s remove lifeline",
                                prolog(__FUNCTION__, __LINE__).c_str(),
                                describeProcId(processor.id).c_str());
      break;
    } else {
      it++;
    }
  }
}


//--------------------------------------------------------------------------
void LifelineMapper::handleLifelineWakeup(const MapperContext ctx,
                                          const MapperMessage& message)
//--------------------------------------------------------------------------
{
  log_lifeline_mapper.debug("%s from %s",
                            prolog(__FUNCTION__, __LINE__).c_str(),
                            describeProcId(message.sender.id).c_str());
  if(quiesced) {
    withdrawActiveLifelines(ctx);
  }
  maybeGetMoreTasks(ctx, message.sender);
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
    case ACTIVATE_LIFELINE:
      handleActivateLifeline(ctx, message);
      break;
    case WITHDRAW_LIFELINE:
      handleWithdrawLifeline(ctx, message);
      break;
    case LIFELINE_WAKEUP:
      handleLifelineWakeup(ctx, message);
      break;
    case RELOCATE_TASK_INFO:
      handleRelocateTaskInfo(ctx, message);
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
std::string LifelineMapper::taskDescription(const Legion::Task& task)
//--------------------------------------------------------------------------
{
  char buffer[512];
  unsigned long long* serialId = (unsigned long long*)task.mapper_data;
  sprintf(buffer, "<%s:%llx:%llx>", task.get_task_name(), *serialId, task.get_unique_id());
  return std::string(buffer);
}


//--------------------------------------------------------------------------
int LifelineMapper::minRunningTasks() const
//--------------------------------------------------------------------------
{
  if(local_proc.kind() == Processor::PY_PROC) {
    return MIN_RUNNING_PY_TASKS;
  } else {
    return MIN_RUNNING_ANALYSIS_TASKS;
  }
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
bool LifelineMapper::isNodeZero() const
//--------------------------------------------------------------------------
{
///DEBUG
log_lifeline_mapper.debug("%s address_space=%d", prolog(__FUNCTION__, __LINE__).c_str(), local_proc.address_space());
  if(total_nodes == 1) return false;
  return local_proc.address_space() == 0;
}



//--------------------------------------------------------------------------
void LifelineMapper::decompose_points(const Rect<1, coord_t> &point_rect,
                                      const Point<1, coord_t> &num_blocks,
                                      std::vector<TaskSlice> &slices)
//--------------------------------------------------------------------------
{
  long long num_points = point_rect.hi - point_rect.lo + Point<1, coord_t>(1);
  Rect<1, coord_t> blocks(Point<1, coord_t>(0), num_blocks - Point<1, coord_t>(1));
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
      Processor target = steal_target_procs[uni(rng)];
      while(target.id == local_proc.id) {
        target = steal_target_procs[uni(rng)];
      }
      slice.proc = target;
      slice.recurse = false;
      slice.stealable = true;
      slices.push_back(slice);
    }
    for(unsigned k = 1; k < TASKS_PER_STEALABLE_SLICE; ++k) pir++;
  }
  
}


//--------------------------------------------------------------------------
void LifelineMapper::slice_task(const MapperContext      ctx,
                                const Task&              task,
                                const SliceTaskInput&    input,
                                SliceTaskOutput&   output)
//--------------------------------------------------------------------------
{
#if 1 // Elliott: Added to ensure tasks never go to node 0
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
#else
  if(isAnalysisTask(task)){
    assert(local_proc.kind() == Processor::Kind::LOC_PROC || local_proc.kind() == Processor::Kind::TOC_PROC);
    sliceTaskCount++;
    Rect<1, coord_t> point_rect = input.domain;
    log_lifeline_mapper.debug("%s task %s target %s %s points %lu",
                              prolog(__FUNCTION__, __LINE__).c_str(),
                              taskDescription(task).c_str(),
                              describeProcId(task.target_proc.id).c_str(),
                              processorKindString(task.target_proc.kind()),
                              point_rect.volume());
    assert(input.domain.get_dim() == 1);
    
    Point<1, coord_t> num_blocks(point_rect.volume() / TASKS_PER_STEALABLE_SLICE);
    decompose_points(point_rect, num_blocks, output.slices);
    slicedPointTaskCount += point_rect.volume();
    
  } else {
    log_lifeline_mapper.debug("%s pass %s to default mapper",
                              prolog(__FUNCTION__, __LINE__).c_str(),
                              taskDescription(task).c_str());
    this->DefaultMapper::slice_task(ctx, task, input, output);
  }
#endif
}

void
LifelineMapper::custom_slice_task(const Task &task,
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
  if (procs.empty()) {
    procs.assign(all_procs.begin(), all_procs.end());
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
                                prolog(__FUNCTION__, __LINE__).c_str(), kind);
      assert(false);
      return (char*)"";
  }
}


//--------------------------------------------------------------------------
bool LifelineMapper::alreadyQueued(const Task* task)
//--------------------------------------------------------------------------
{
  std::set<const Task*>::iterator workerFinder = worker_ready_queue.find(task);
  if (workerFinder != worker_ready_queue.end()) return true;
  std::set<const Task*>::iterator relocatedFinder = relocated_tasks.find(task);
  if (relocatedFinder != relocated_tasks.end()) return true;
  std::set<const Task*>::iterator localMapFinder = tasks_to_map_locally.find(task);
  if (localMapFinder != tasks_to_map_locally.end()) return true;
  return false;
}

//--------------------------------------------------------------------------
bool LifelineMapper::send_stolen_tasks(MapperContext ctx,
                                       const SelectMappingInput&    input,
                                       SelectMappingOutput&   output)
//--------------------------------------------------------------------------
{
  bool relocated = false;
  std::vector<std::pair<Processor, unsigned>> thiefProcs;
  
  for(SendQueue::iterator it = send_queue.begin();
      it != send_queue.end(); it++) {
    Processor processor = it->first;
    std::vector<const Task*> tasks = it->second;
    
    if(tasks.size() > 0) {
      thiefProcs.push_back(std::make_pair(processor, (unsigned)tasks.size()));
      
      for(std::vector<const Task*>::const_iterator taskIt = tasks.begin();
          taskIt != tasks.end(); ++taskIt) {
        const Task* task = *taskIt;
        output.relocate_tasks[task] = processor;
        relocated = true;
        std::set<const Task*>::iterator findIt = relocated_tasks.find(task);
        relocated_tasks.erase(findIt);
        log_lifeline_mapper.debug("%s send task %s to %s",
                                  prolog(__FUNCTION__, __LINE__).c_str(),
                                  taskDescription(*task).c_str(),
                                  describeProcId(processor.id).c_str());
      }
    }
  }
  
  send_queue.clear();
  
  for(std::vector<std::pair<Processor, unsigned>>::iterator it = thiefProcs.begin();
      it != thiefProcs.end(); it++) {
    Processor processor = it->first;
    unsigned numTasks = it->second;
    Request r = { local_proc, processor, numTasks };
    log_lifeline_mapper.debug("%s send STEAL_ACK to %s",
                              prolog(__FUNCTION__, __LINE__).c_str(),
                              describeProcId(processor.id).c_str());
    runtime->send_message(ctx, processor, &r, sizeof(r), STEAL_ACK);
  }
  return relocated;
}

//--------------------------------------------------------------------------
void LifelineMapper::giveLifeThroughLifelines(const MapperContext          ctx)
//--------------------------------------------------------------------------
{
  for(std::vector<Processor>::iterator it = active_lifelines.begin();
      it != active_lifelines.end(); ) {
    Processor processor = *it;
    log_lifeline_mapper.debug("%s send LIFELINE_WAKEUP to %s",
                              prolog(__FUNCTION__, __LINE__).c_str(),
                              describeProcId(processor.id).c_str());
    runtime->send_message(ctx, processor, NULL, 0, LIFELINE_WAKEUP);
    it = active_lifelines.erase(it);
  }
}

//--------------------------------------------------------------------------
void LifelineMapper::processNewReadyTasks(const SelectMappingInput&    input,
                                          SelectMappingOutput&   output,
                                          bool& mappedOrRelocated,
                                          bool& sawWorkerReadyTasks)
//--------------------------------------------------------------------------
{
  for (std::list<const Task*>::const_iterator it = input.ready_tasks.begin();
       it != input.ready_tasks.end(); it++) {
    const Task* task = *it;
    if(!alreadyQueued(task)) {
      bool mapHereNow = false;
log_lifeline_mapper.debug("%s mapping herenow ? locallyRunningTaskCount %d isNodeZero %d", prolog(__FUNCTION__, __LINE__).c_str(), locallyRunningTaskCount(), isNodeZero());
      if(locallyRunningTaskCount() < minRunningTasks() && !isNodeZero()) {
        mapHereNow = true;
      } else if(!isAnalysisTask(*task)) {
        mapHereNow = true;
      }

      if(mapHereNow) {
        output.map_tasks.insert(task);
        if(isAnalysisTask(*task)) {
          locallyStartedTaskCount++;
          
        }
        mappedOrRelocated = isAnalysisTask(*task);
        log_lifeline_mapper.debug("%s select task %s for here now",
                                  prolog(__FUNCTION__, __LINE__).c_str(),
                                  taskDescription(*task).c_str());
      } else {
        worker_ready_queue.insert(task);
        sawWorkerReadyTasks = isAnalysisTask(*task);
        log_lifeline_mapper.debug("%s move task %s to worker_ready_queue",
                                  prolog(__FUNCTION__, __LINE__).c_str(),
                                  taskDescription(*task).c_str());
      }
    }
  }
}


//--------------------------------------------------------------------------
void LifelineMapper::select_tasks_to_map(const MapperContext          ctx,
                                         const SelectMappingInput&    input,
                                         SelectMappingOutput&   output)
//--------------------------------------------------------------------------
{
  log_lifeline_mapper.debug("%s relocated_tasks %ld "
                            "worker_ready_queue %ld "
                            "input.ready_tasks %ld "
                            "tasks_to_map_locally %ld",
                            prolog(__FUNCTION__, __LINE__).c_str(),
                            relocated_tasks.size(),
                            worker_ready_queue.size(),
                            input.ready_tasks.size(),
                            tasks_to_map_locally.size());
  log_lifeline_mapper.debug("%s %s",
                            prolog(__FUNCTION__, __LINE__).c_str(),
                            workloadState().c_str());
  
  if(defer_select_tasks_to_map.exists()) {
    triggerSelectTasksToMap(ctx);
  }
  bool mappedOrRelocated = false;
  bool sawWorkerReadyTasks = false;
  
  processNewReadyTasks(input, output, mappedOrRelocated, sawWorkerReadyTasks);
  
  for(std::set<const Task*>::iterator it = tasks_to_map_locally.begin();
      it != tasks_to_map_locally.end(); ++it) {
    const Task* task = *it;
    output.map_tasks.insert(task);
    log_lifeline_mapper.debug("%s select local task %s for here now",
                              prolog(__FUNCTION__, __LINE__).c_str(),
                              taskDescription(*task).c_str());
  }
  tasks_to_map_locally.clear();
  
#if 1
if(local_proc.kind() == Processor::TOC_PROC || local_proc.kind() == Processor::LOC_PROC) {
  if(quiesced) {
    log_lifeline_mapper.debug("%s quiesced, should wakeup? mappedOrRelocated %d sawWorkerReadyTasks %d",
prolog(__FUNCTION__, __LINE__).c_str(), mappedOrRelocated, sawWorkerReadyTasks);
  }
}
#endif

  if(quiesced && (mappedOrRelocated || sawWorkerReadyTasks)) {
    withdrawActiveLifelines(ctx);
  }

#if 1
if(local_proc.kind() == Processor::TOC_PROC || local_proc.kind() == Processor::LOC_PROC) {
log_lifeline_mapper.debug("%s give life? active_lifelines.empty? %d worker_ready_queue.size %ld",
prolog(__FUNCTION__, __LINE__).c_str(), active_lifelines.empty(), worker_ready_queue.size());
}
#endif
  
  if(!active_lifelines.empty() && worker_ready_queue.size() > 0) {
    giveLifeThroughLifelines(ctx);
  }
  
  mappedOrRelocated |= send_stolen_tasks(ctx, input, output);
  
#if 1
  {
#else
  if (!mappedOrRelocated && !input.ready_tasks.empty()) {
#endif
    log_lifeline_mapper.debug("%s create mapper event for reinvoking",
                              prolog(__FUNCTION__, __LINE__).c_str());
    if (!defer_select_tasks_to_map.exists()) {
      defer_select_tasks_to_map = runtime->create_mapper_event(ctx);
    }
    output.deferral_event = defer_select_tasks_to_map;
  }
}



//--------------------------------------------------------------------------
void LifelineMapper::select_steal_targets(const MapperContext         ctx,
                                          const SelectStealingInput&  input,
                                          SelectStealingOutput& output)
//--------------------------------------------------------------------------
{
  log_lifeline_mapper.debug("%s", prolog(__FUNCTION__, __LINE__).c_str());
  maybeGetMoreTasks(ctx);
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
                                                 regions, result, created, true/*acquire*/, GC_FIRST_PRIORITY)) {
    log_lifeline_mapper.error("%s Lifeline mapper failed to allocate instance", prolog(__FUNCTION__, __LINE__).c_str());
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
  if(isAnalysisTask(task)) {
    locallyEndedTaskCount++;
  }
  
  Realm::ProfilingMeasurements::OperationTimeline timeline;
  input.profiling_responses.get_measurement<Realm::ProfilingMeasurements::OperationTimeline>(timeline);
  Realm::ProfilingMeasurements::OperationTimeline::timestamp_t elapsedNS = timeline.end_time - timeline.start_time;
  
  log_lifeline_mapper.info("%s report task complete %s %lld totalPendingWorkload %d locallyRunning %d",
                            prolog(__FUNCTION__, __LINE__).c_str(),
                            taskDescription(task).c_str(),
                            elapsedNS,
                            totalPendingWorkload(),
                            locallyRunningTaskCount());
  
  maybeGetMoreTasks(ctx);
}

//--------------------------------------------------------------------------
void LifelineMapper::map_task(const MapperContext      ctx,
                              const Task&              task,
                              const MapTaskInput&      input,
                              MapTaskOutput&     output)
//--------------------------------------------------------------------------
{
  Processor::Kind target_kind = task.target_proc.kind();
  VariantInfo chosen = default_find_preferred_variant(task, ctx,
                                                      true/*needs tight bound*/, false/*cache*/, target_kind);
  output.chosen_variant = chosen.variant;
  output.task_priority = 0;
  output.postmap_task = false;
  
  std::vector<Processor> procsToInsert;
  switch(local_proc.kind()) {
    case Processor::LOC_PROC:
      procsToInsert = local_cpus;
      break;
    case Processor::TOC_PROC:
      procsToInsert.push_back(task.target_proc); // Can't make a processor group with GPUs
      break;
    case Processor::IO_PROC:
      procsToInsert = local_ios;
      break;
    case Processor::PY_PROC:
      procsToInsert = local_pys;
      break;
    default: log_lifeline_mapper.fatal("mapping a task onto processor type %d", local_proc.kind());
  }
  output.target_procs.clear();
  output.target_procs.insert(output.target_procs.end(), procsToInsert.begin(), procsToInsert.end());

  if(task.orig_proc.id == local_proc.id) {
    
    mappedSelfGeneratedTaskCount++;
    
    log_lifeline_mapper.debug("%s maps self task %s %s",
                              prolog(__FUNCTION__, __LINE__).c_str(),
                              taskDescription(task).c_str(),
                              workloadState().c_str());
  } else {
    
    mappedRelocatedTaskCount++;
    
    log_lifeline_mapper.debug("%s maps relocated (from %s) task %s %s",
                              prolog(__FUNCTION__, __LINE__).c_str(),
                              describeProcId(task.orig_proc.id).c_str(),
                              taskDescription(task).c_str(),
                              workloadState().c_str());
  }
  
  ProfilingRequest completionRequest;
  completionRequest.add_measurement<Realm::ProfilingMeasurements::OperationStatus>();
  completionRequest.add_measurement<Realm::ProfilingMeasurements::OperationTimeline>();
  output.task_prof_requests = completionRequest;
  
  for (unsigned idx = 0; idx < task.regions.size(); idx++) {
    if (task.regions[idx].privilege == NO_ACCESS)
      continue;
    Memory target_mem = default_policy_select_target_memory(ctx,
                                                            task.target_proc,
                                                            task.regions[idx]);
    map_task_array(ctx, task.regions[idx].region, target_mem,
                   output.chosen_instances[idx]);
  }
  runtime->acquire_instances(ctx, output.chosen_instances);
  
}


//--------------------------------------------------------------------------
Legion::Processor LifelineMapper::nearestProcessor(unsigned kind)
//--------------------------------------------------------------------------
{
  switch(kind) {
    case LOC_PROC:
      return nearestLOCProc;
      break;
    case TOC_PROC:
      return nearestTOCProc;
      break;
    case IO_PROC:
      return nearestIOProc;
      break;
    case PY_PROC:
      return nearestPYProc;
      break;
    default: assert(false);
  }
}
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
Legion::Processor LifelineMapper::randomProcessor(unsigned kind)
//--------------------------------------------------------------------------
{
  
  switch(kind) {
    case LOC_PROC:
      return LOCProcs[uniLOC(rng)];
      break;
    case TOC_PROC:
      return TOCProcs[uniTOC(rng)];
      break;
    case PY_PROC:
      return nearestProcessor(kind);
      break;
    default: assert(false);
  }
  
}

//--------------------------------------------------------------------------
void LifelineMapper::assignTaskId(const MapperContext    ctx,
                                  const Task& task)
//--------------------------------------------------------------------------
{
  size_t shiftBits = sizeof(taskSerialId) * sizeof(char);
  unsigned long long taskId = (local_proc.id << shiftBits) + taskSerialId++;
  runtime->update_mappable_data(ctx, task, &taskId, sizeof(taskId));
}

//--------------------------------------------------------------------------
void LifelineMapper::select_task_options(const MapperContext    ctx,
                                         const Task&            task,
                                         TaskOptions&     output)
//--------------------------------------------------------------------------
{
  assignTaskId(ctx, task);
  DefaultMapper::VariantInfo variantInfo =
  DefaultMapper::default_find_preferred_variant(task, ctx,
                                                /*needs tight bound*/false,
                                                /*cache result*/true,
                                                Processor::NO_KIND);
  assert(variantInfo.proc_kind != Processor::NO_KIND);
  bool sendRelocateTaskInfo = false;

  Processor initial_proc;
  if(variantInfo.proc_kind == local_proc.kind()) {
    initial_proc = local_proc;
    selfGeneratedTaskCount++;
  } else {
    sendRelocateTaskInfo = true;
    initial_proc = randomProcessor(variantInfo.proc_kind);
  }
  
  assert(initial_proc.exists());
  output.initial_proc = initial_proc;
  output.inline_task = false;
  output.stealable = false;
  output.map_locally = false;

  log_lifeline_mapper.debug("%s %s on %s %s parent task %s",
                            prolog(__FUNCTION__, __LINE__).c_str(),
                            taskDescription(task).c_str(),
                            processorKindString(initial_proc.kind()),
                            workloadState().c_str(),
                            task.parent_task == NULL ? "<none>" :
                              taskDescription(*task.parent_task).c_str());
  
  if(sendRelocateTaskInfo) {
    Request r = { initial_proc, local_proc, 1 };
    log_lifeline_mapper.debug("%s send RELOCATE_TASK_INFO to %s",
                              prolog(__FUNCTION__, __LINE__).c_str(),
                              describeProcId(initial_proc.id).c_str());
    runtime->send_message(ctx, initial_proc, &r, sizeof(r), RELOCATE_TASK_INFO);
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
void LifelineMapper::premap_task(const MapperContext      ctx,
                                 const Task&              task,
                                 const PremapTaskInput&   input,
                                 PremapTaskOutput&        output)
//------------------------------------------------------------------------
{
  log_lifeline_mapper.debug("%s premap_task %s",
                            prolog(__FUNCTION__, __LINE__).c_str(),
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
  std::map<Processor, Memory>* proc_fbmems = new std::map<Processor, Memory>();

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
    if (affinity.p.kind() == Processor::TOC_PROC) {
      if (affinity.m.kind() == Memory::GPU_FB_MEM) {
        (*proc_fbmems)[affinity.p] = affinity.m;
      }
    }
  }
  
  for (std::map<Processor, Memory>::iterator it = proc_sysmems->begin();
       it != proc_sysmems->end(); ++it) {
    procs_list->push_back(it->first);
    (*sysmem_local_procs)[it->second].push_back(it->first);
  }

  for (std::map<Processor, Memory>::iterator it = proc_fbmems->begin();
       it != proc_fbmems->end(); ++it) {
    procs_list->push_back(it->first);
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
