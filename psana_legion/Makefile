# Copyright 2018 Stanford University
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#


ifndef LG_RT_DIR
$(error LG_RT_DIR variable is not defined, aborting build)
endif

# Flags for directing the runtime makefile what to include
# FIXME: DEBUG=1 causes a crash because the postamble attempts to take a lock and causes the task to block
DEBUG           ?= 1		# Include debugging symbols
OUTPUT_LEVEL    ?= LEVEL_DEBUG	# Compile time logging level
USE_CUDA        ?= 0		# Include CUDA support (requires CUDA)
USE_GASNET      ?= 0		# Include GASNet support (requires GASNet)
USE_HDF         ?= 0		# Include HDF5 support (requires HDF5)
ALT_MAPPERS     ?= 0		# Include alternative mappers (not recommended)

USE_PYTHON := 1

# Hack: Create a dynamic library here so that Legion can be linked independently
OUTFILE		?= libpsana_legion.so
# List all the application source files here
GEN_SRC		?=		# .cc files
GEN_GPU_SRC	?=		# .cu files

# You can modify these variables, some will be appended to by the runtime makefile
INC_FLAGS	?=
CC_FLAGS	?=
NVCC_FLAGS	?=
GASNET_FLAGS	?=
LD_FLAGS	?=

CC_FLAGS += -DPYTHON_MODULES_PATH="\"$(LG_RT_DIR)/../bindings/python:$(dir $(abspath $(firstword $(MAKEFILE_LIST))))\""

# These flags force GCC 5 to be compatible with GCC 4.9 and
# earlier. Required to link against pre-build psana binaries.
CC_FLAGS += -std=c++11 -fabi-version=2 -D_GLIBCXX_USE_CXX11_ABI=0

CC_FLAGS += -fPIC
ifeq ($(shell uname), Darwin)
	LD_FLAGS += -dynamiclib -single_module -undefined dynamic_lookup -fPIC
	CORE_LD_FLAGS += -dynamiclib -single_module -undefined dynamic_lookup -fPIC
else
	LD_FLAGS += -shared
	CORE_LD_FLAGS += -shared
endif

ifeq ($(shell uname), Darwin)
	LD_FLAGS += -Wl,-force_load,liblegion.a
else
	LD_FLAGS += -Wl,--whole-archive -llegion -Wl,--no-whole-archive
endif

ifneq ($(CRAYPE_VERSION),)
	BIN_LD_FLAGS += -dynamic
	BIN_LD_FLAGS += ${CRAY_UGNI_POST_LINK_OPTS}
	BIN_LD_FLAGS += ${CRAY_PMI_POST_LINK_OPTS}
	BIN_LIBS += -lugni -lpmi -lhugetlbfs
endif

# Create core library that both Legion and MPI can link against
CORE_OUTFILE ?= libnative_kernels_core.so
CORE_GEN_OBJ ?= native_kernels.o native_kernels_helper.o

# Create the real binary
BIN_OUTFILE ?= psana_legion
BIN_GEN_OBJ ?= psana_legion.o native_kernels_tasks.o simple_mapper.o lifeline_mapper.o
ifeq ($(strip $(USE_CUDA)),1)
BIN_GEN_GPU_OBJ ?= native_kernels_tasks_gpu.o
else
BIN_GEN_GPU_OBJ :=
endif

.PHONY: all
all: $(OUTFILE) $(CORE_OUTFILE) $(BIN_OUTFILE)

$(CORE_GEN_OBJ) : %.o : %.cc
	$(CXX) -o $@ -c $< $(CC_FLAGS) $(INC_FLAGS) -O3
$(CORE_OUTFILE) : $(CORE_GEN_OBJ)
	@echo "---> Linking objects into one binary: $(CORE_OUTFILE)"
	$(CXX) -o $(CORE_OUTFILE) $(CORE_GEN_OBJ) $(CORE_LD_FLAGS)

$(BIN_GEN_OBJ) : %.o : %.cc
	$(CXX) -o $@ -c $< $(CC_FLAGS) $(INC_FLAGS)
$(BIN_GEN_GPU_OBJ) : %.o : %.cu
	$(NVCC) -o $@ -c $< $(NVCC_FLAGS) $(INC_FLAGS)
$(BIN_OUTFILE) : $(BIN_GEN_OBJ) $(BIN_GEN_GPU_OBJ) $(OUTFILE) $(CORE_OUTFILE)
	@echo "---> Linking objects into one binary: $(BIN_OUTFILE)"
	$(CXX) -o $(BIN_OUTFILE) $(BIN_GEN_OBJ) $(BIN_GEN_COBJ) $(BIN_LD_FLAGS) -L. -lpsana_legion -lnative_kernels_core $(BIN_LIBS)

.PHONY: pyext
pyext: $(CORE_OUTFILE)
	python setup.py install

###########################################################################
#
#   Don't change anything below here
#   
###########################################################################

include $(LG_RT_DIR)/runtime.mk

clean::
	$(RM) -f $(OUTFILE) $(SLIB_LEGION) $(SLIB_REALM) $(GEN_OBJS) $(GEN_GPU_OBJS) $(LOW_RUNTIME_OBJS) $(HIGH_RUNTIME_OBJS) $(GPU_RUNTIME_OBJS) $(MAPPER_OBJS) $(ASM_OBJS) $(CORE_OUTFILE) $(CORE_GEN_OBJ) $(BIN_OUTFILE) $(BIN_GEN_OBJ)
