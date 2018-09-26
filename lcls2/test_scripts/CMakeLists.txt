cmake_minimum_required(VERSION 3.1)
project(native_kernels)

find_package(Legion REQUIRED)

list(APPEND NATIVE_KERNELS_SRC
  ../../psana_legion/native_kernels.cc        ../../psana_legion/native_kernels.h
  ../../psana_legion/native_kernels_helper.cc ../../psana_legion/native_kernels_helper.h
  ../../psana_legion/native_kernels_tasks.cc  ../../psana_legion/native_kernels_tasks.h
)

add_library(native_kernels SHARED ${NATIVE_KERNELS_SRC})
target_link_libraries(native_kernels Legion::Legion)
