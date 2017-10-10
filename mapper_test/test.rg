-- Copyright 2017 Stanford University
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.

import "regent"
import "bishop"

local cmapper = require("build_mapper")

local c = regentlib.c
local cb = bishoplib.c

struct elt { x : int }

task fetch(event : int, data : region(ispace(int2d), elt))
where reads writes(data) do
  var entry = c.legion_get_current_time_in_nanos()
  var duration = 0.01 * float(c.random()) / float(c.RAND_MAX);
  c.legion_runtime_get_executing_processor(__runtime(), __context())
  var proc = c.legion_runtime_get_executing_processor(__runtime(), __context())
  var exit = c.legion_get_current_time_in_nanos()
  while (exit - entry) < duration * 10000000000L do
    exit = c.legion_get_current_time_in_nanos()
  end
  var elapsed = exit - entry
  c.printf("%ld fetch %d p %d %ld\n", entry,
    event, proc.id, elapsed)
end

task analyze(event : int, data : region(ispace(int2d), elt))
where reads(data) do
  var entry = c.legion_get_current_time_in_nanos()
  var duration = 0.01 * float(c.random()) / float(c.RAND_MAX);
  c.legion_runtime_get_executing_processor(__runtime(), __context())
  var proc = c.legion_runtime_get_executing_processor(__runtime(), __context())
  var exit = c.legion_get_current_time_in_nanos()
  while (exit - entry) < duration * 10000000000L do
    exit = c.legion_get_current_time_in_nanos()
  end
  var elapsed = exit - entry
  c.printf("%ld analyze %d p %d %ld\n", entry,
    event, proc.id, elapsed)
end

task fetch_and_analyze(event : int)
  var entry = c.legion_get_current_time_in_nanos()
  var duration = 0.01 * float(c.random()) / float(c.RAND_MAX);
  c.legion_runtime_get_executing_processor(__runtime(), __context())
  var proc = c.legion_runtime_get_executing_processor(__runtime(), __context())
  var data = region(ispace(int2d, { 10, 10 }), elt)
  fetch(event, data)
  analyze(event, data)
  __delete(data)
  var exit = c.legion_get_current_time_in_nanos()
  while (exit - entry) < duration * 10000000000L do
    exit = c.legion_get_current_time_in_nanos()
  end
  var elapsed = exit - entry
  c.printf("%ld fetch_and_analyze %d p %d %ld\n", entry,
    event, proc.id, elapsed)
end

task dummy()
  return c.legion_get_current_time_in_nanos()
end

task main()
  c.printf("%ld enter main\n", c.legion_get_current_time_in_nanos())
  c.srandom(0)
  var nevents_total = 1000
  var nevents_per_launch = 100

  for launch_offset = 0, nevents_total, nevents_per_launch do
    __demand(__parallel)
    for index = 0, min(nevents_per_launch, nevents_total - launch_offset) do
      fetch_and_analyze(launch_offset + index)
    end
  end
  c.legion_runtime_issue_execution_fence(__runtime(), __context())
  var d = dummy()
  c.printf("%ld exit main\n", d)
end
regentlib.start(main, cmapper.register_mappers)
