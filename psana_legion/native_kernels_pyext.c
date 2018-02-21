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

#include "native_kernels.h"

#include <Python.h>

static PyObject *pyext_memory_bound_kernel(PyObject *self, PyObject *args)
{
  memory_bound_kernel_default();
  Py_RETURN_NONE;
}

static PyObject *pyext_cache_bound_kernel(PyObject *self, PyObject *args)
{
  cache_bound_kernel_default();
  Py_RETURN_NONE;
}

static PyMethodDef PyextMethods[] = {
  {"memory_bound_kernel", pyext_memory_bound_kernel, METH_NOARGS,
   "Execute memory-bound kernel."},
  {"cache_bound_kernel", pyext_cache_bound_kernel, METH_NOARGS,
   "Execute cache-bound kernel."},
  {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC initnative_kernels(void)
{
  (void) Py_InitModule("native_kernels", PyextMethods);
}
