/* Copyright 2019 Stanford University
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

#include "legion.h"
#include "realm/python/python_module.h"
#include "realm/python/python_source.h"

#ifdef REALM_USE_SUBPROCESSES
#include "realm/custom_malloc.h"
#include "realm/runtime_impl.h"
#define INSTALL_REALM_ALLOCATOR Realm::ScopedAllocatorPush sap(Realm::RuntimeImpl::realm_allocator)
#else
#define INSTALL_REALM_ALLOCATOR do {} while (0)
#endif

#include "native_kernels_tasks.h"
#include "io_tasks.h"

#include "simple_mapper.h"
#include "lifeline_mapper.h"

#ifdef PSANA_USE_MPI
#include "mpi.h"
#endif

using namespace Legion;

enum TaskIDs {
  TOP_LEVEL_TASK_ID = 1,
  MEMORY_BOUND_TASK_ID = 2,
  CACHE_BOUND_TASK_ID = 3,
  SUM_TASK_ID = 4,
};

int main(int argc, char **argv)
{
#ifdef PSANA_USE_MPI
  // Call MPI_Init here so that it happens before gasnet_init
  // Needed to avoid conflict between MPI and GASNet on Cray systems
  MPI_Init(&argc, &argv);
#endif

  // do this before any threads are spawned
#ifdef PYTHON_MODULES_PATH
  char *previous_python_path = getenv("PYTHONPATH");
  if (previous_python_path != 0) {
    size_t bufsize = 16 * 1024;
    char *buffer = (char *)calloc(bufsize, sizeof(char));
    assert(buffer != 0);

    assert(strlen(previous_python_path) + strlen(PYTHON_MODULES_PATH) + 2 < bufsize);
    // Concatenate PYTHON_MODULES_PATH to the end of PYTHONPATH.
    bufsize--;
    strncat(buffer, previous_python_path, bufsize);
    bufsize -= strlen(previous_python_path);
    strncat(buffer, ":" PYTHON_MODULES_PATH, bufsize);
    bufsize -= strlen(":" PYTHON_MODULES_PATH);
    setenv("PYTHONPATH", buffer, true /*overwrite*/);
  } else {
    setenv("PYTHONPATH", PYTHON_MODULES_PATH, true /*overwrite*/);
  }
#endif

  const char *module = getenv("PSANA_MODULE");
  if (!module) {
    module = "user";
  }
  Realm::Python::PythonModule::import_python_module(module);

  Runtime::set_top_level_task_id(TOP_LEVEL_TASK_ID);

  {
    INSTALL_REALM_ALLOCATOR;
    preregister_native_kernels_tasks(MEMORY_BOUND_TASK_ID,
                                     CACHE_BOUND_TASK_ID,
                                     SUM_TASK_ID);
#ifdef REALM_USE_SUBPROCESSES
    preregister_io_tasks();
#endif
  }

  char *mapper = getenv("PSANA_MAPPER");
  if (mapper && strcmp(mapper, "simple") == 0) {
    preregister_simple_mapper();
  } else if (mapper && strcmp(mapper, "lifeline") == 0) {
    preregister_lifeline_mapper();
  } else {
    fprintf(stderr, "Error: PSANA_MAPPER is not set.\n");
    exit(1);
  }

  Runtime::start(argc, argv);

#ifdef PSANA_USE_MPI
  MPI_Finalize();
#endif
  return 0;
}
