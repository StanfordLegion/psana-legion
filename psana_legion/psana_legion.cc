/* Copyright 2018 Stanford University
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

#include "native_kernels_tasks.h"

#include "simple_mapper.h"
#include "task_pool_mapper.h"
#include "lifeline_mapper.h"

using namespace Legion;

enum TaskIDs {
  TOP_LEVEL_TASK_ID = 1,
  MAIN_TASK_ID = 2,
  MEMORY_BOUND_TASK_ID = 3,
  CACHE_BOUND_TASK_ID = 4,
};

enum FieldIDs {
  X_FIELD_ID = 1,
};

VariantID preregister_python_task_variant(
  const TaskVariantRegistrar &registrar,
  const char *module_name,
  const char *function_name,
  const void *userdata = NULL,
  size_t userlen = 0)
{
  CodeDescriptor code_desc(Realm::Type::from_cpp_type<Processor::TaskFuncPtr>());
  code_desc.add_implementation(new Realm::PythonSourceImplementation(module_name, function_name));

  return Runtime::preregister_task_variant(
    registrar, code_desc, userdata, userlen,
    registrar.task_variant_name);
}

int main(int argc, char **argv)
{
  // do this before any threads are spawned
#ifndef PYTHON_MODULES_PATH
#error PYTHON_MODULES_PATH not available at compile time
#endif
  char *previous_python_path = getenv("PYTHONPATH");
  if (previous_python_path != 0) {
    size_t bufsize = 8192;
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

  const char *module = getenv("PSANA_MODULE");
  if (!module) {
    module = "user";
  }
  Realm::Python::PythonModule::import_python_module(module);

  {
    TaskVariantRegistrar registrar(MAIN_TASK_ID, "main_task");
    registrar.add_constraint(ProcessorConstraint(Processor::PY_PROC));
    preregister_python_task_variant(registrar, "psana_legion", "main_task");
  }

  Runtime::set_top_level_task_id(MAIN_TASK_ID);

  register_native_kernels_tasks(MEMORY_BOUND_TASK_ID,
                                CACHE_BOUND_TASK_ID);

  char *mapper = getenv("PSANA_MAPPER");
  if (mapper && strcmp(mapper, "simple") == 0) {
    register_simple_mapper();
  } else if (mapper && strcmp(mapper, "task_pool") == 0) {
    register_task_pool_mapper();
  } else if (mapper && strcmp(mapper, "lifeline") == 0) {
    register_lifeline_mapper();
  } else {
    fprintf(stderr, "Error: PSANA_MAPPER is not set.\n");
    exit(1);
  }

  return Runtime::start(argc, argv);
}
