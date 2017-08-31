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
  c.printf("fetch %d\n", event)
end

task analyze(event : int, data : region(ispace(int2d), elt))
where reads(data) do
  c.printf("analyze %d\n", event)
end

task fetch_and_analyze(event : int)
var proc =
  c.legion_runtime_get_executing_processor(__runtime(), __context())
  var procs = cb.bishop_all_processors()

  c.printf("fetch_and_analyze %d on proc %d\n", event, procs.list[1].id)
  var data = region(ispace(int2d, { 10, 10 }), elt)
  fetch(event, data)
  analyze(event, data)
  __delete(data)
end

task main()
  var nevents_total = 1000
  var nevents_per_launch = 100

  for launch_offset = 0, nevents_total, nevents_per_launch do
    __demand(__parallel)
    for index = 0, min(nevents_per_launch, nevents_total - launch_offset) do
      fetch_and_analyze(launch_offset + index)
    end
  end
end
regentlib.start(main, cmapper.register_mappers)
