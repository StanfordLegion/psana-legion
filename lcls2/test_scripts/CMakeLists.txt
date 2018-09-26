cmake_minimum_required(VERSION 3.1)
project(native_kernels)

find_package(Legion REQUIRED)

list(APPEND NATIVE_KERNELS_SRC
  native_kernels.cc        native_kernels.h
  native_kernels_helper.cc native_kernels_helper.h
  native_kernels_tasks.cc  native_kernels_tasks.h
)

add_library(native_kernels SHARED ${NATIVE_KERNELS_SRC})
target_link_libraries(native_kernels Legion::Legion)
