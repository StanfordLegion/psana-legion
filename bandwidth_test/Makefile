# Copyright 2019 Stanford University
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
DEBUG           ?= 1		# Include debugging symbols
OUTPUT_LEVEL    ?= LEVEL_DEBUG	# Compile time logging level
USE_CUDA        ?= 0		# Include CUDA support (requires CUDA)
USE_GASNET      ?= 0		# Include GASNet support (requires GASNet)
USE_HDF         ?= 0		# Include HDF5 support (requires HDF5)
ALT_MAPPERS     ?= 0		# Include alternative mappers (not recommended)

# Hack: Create a dynamic library here so that Legion can be linked independently
OUTFILE		?= test_legion
# List all the application source files here
GEN_SRC		?= test_legion.cc		# .cc files
GEN_GPU_SRC	?=		# .cu files

# You can modify these variables, some will be appended to by the runtime makefile
INC_FLAGS	?=
CC_FLAGS	?=
NVCC_FLAGS	?=
GASNET_FLAGS	?=
LD_FLAGS	?=

# Create core library that both Legion and MPI can link against
CORE_GEN_OBJ ?= native_kernels.o native_kernels_helper.o
MPI_GEN_OBJ ?= test_mpi.o
MPI_OUTFILE ?= test_mpi
MPI_OPENMP_GEN_OBJ ?= test_mpi_openmp.o
MPI_OPENMP_OUTFILE ?= test_mpi_openmp

ifeq ($(CRAYPE_VERSION),)
	MPI_LD_FLAGS += -lmpi
endif

.PHONY: all
all: $(OUTFILE) $(MPI_OUTFILE) $(MPI_OPENMP_OUTFILE)

$(CORE_GEN_OBJ) : %.o : %.cc
	$(CXX) -o $@ -c $< $(CC_FLAGS) $(INC_FLAGS) -O3
$(MPI_GEN_OBJ) : %.o : %.cc
	$(CXX) -o $@ -c $< $(CC_FLAGS) $(INC_FLAGS)
$(MPI_OPENMP_GEN_OBJ) : %.o : %.cc
	$(CXX) -o $@ -c $< $(CC_FLAGS) $(INC_FLAGS) -fopenmp

$(MPI_OUTFILE) : $(CORE_GEN_OBJ) $(MPI_GEN_OBJ)
	@echo "---> Linking objects into one binary: $(MPI_OUTFILE)"
	$(CXX) -o $(MPI_OUTFILE) $(CORE_GEN_OBJ) $(MPI_GEN_OBJ) $(MPI_LD_FLAGS)

$(MPI_OPENMP_OUTFILE) : $(CORE_GEN_OBJ) $(MPI_OPENMP_GEN_OBJ)
	@echo "---> Linking objects into one binary: $(MPI_OPENMP_OUTFILE)"
	$(CXX) -o $(MPI_OPENMP_OUTFILE) $(CORE_GEN_OBJ) $(MPI_OPENMP_GEN_OBJ) $(MPI_LD_FLAGS) -fopenmp

NO_BUILD_ALL=1

###########################################################################
#
#   Don't change anything below here
#   
###########################################################################

include $(LG_RT_DIR)/runtime.mk

$(OUTFILE) : $(GEN_OBJS) $(CORE_GEN_OBJ) $(SLIB_LEGION) $(SLIB_REALM)
	@echo "---> Linking objects into one binary: $(OUTFILE)"
	$(CXX) -o $(OUTFILE) $(GEN_OBJS) $(CORE_GEN_OBJ) $(LD_FLAGS) $(LEGION_LIBS) $(LEGION_LD_FLAGS) $(GASNET_FLAGS)

clean::
	$(RM) -f $(OUTFILE) $(MPI_OUTFILE) $(SLIB_LEGION) $(SLIB_REALM) $(GEN_OBJS) $(GEN_GPU_OBJS) $(LOW_RUNTIME_OBJS) $(HIGH_RUNTIME_OBJS) $(GPU_RUNTIME_OBJS) $(MAPPER_OBJS) $(ASM_OBJS) $(CORE_GEN_OBJ) $(MPI_GEN_OBJ)
