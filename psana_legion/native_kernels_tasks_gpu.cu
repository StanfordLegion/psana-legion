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

#include "native_kernels.h"

#include <stdint.h>
#include <inttypes.h>

using namespace Legion;


__global__
void gpu_sum_kernel(Rect<3> rect,
                    const FieldAccessor<READ_ONLY, int16_t, 3, coord_t, Realm::AffineAccessor<int16_t, 3, coord_t> > x,
                    unsigned long long *result)
{
  const int idx = blockIdx.x * blockDim.x + threadIdx.x;
  const int idy = blockIdx.y * blockDim.y + threadIdx.y;
  const int idz = blockIdx.z * blockDim.z + threadIdx.z;
  const Point<3> p(rect.lo.x + idx, rect.lo.y + idy, rect.lo.z + idz);

  // WARNING: This kernel is really, really inefficient. Please don't
  // use this in any context where performance is important!!!

  // FIXME: CUDA only supports atomicAdd on unsigned. Hopefully this
  // cast does sign extension???
  unsigned long long value = x[p];
  atomicAdd(result, value);
}

__host__
int64_t gpu_sum_task(const Task *task,
                     const std::vector<PhysicalRegion> &regions,
                     Context ctx, Runtime *runtime)
{
  assert(regions.size() == 1);

  const FieldAccessor<READ_ONLY, int16_t, 3, coord_t, Realm::AffineAccessor<int16_t, 3, coord_t> > x(regions[0], X_FIELD_ID);

  Rect<3> rect = runtime->get_index_space_domain(ctx,
                  regions[0].get_logical_region().get_index_space());

  const dim3 block(8, 8, 4);
  const dim3 grid(
    ((rect.hi.x - rect.lo.x + 1) + (block.x-1)) / block.x,
    ((rect.hi.y - rect.lo.y + 1) + (block.y-1)) / block.y,
    ((rect.hi.z - rect.lo.z + 1) + (block.z-1)) / block.z);

  unsigned long long result = 0;

  unsigned long long *gpu_result;
  if (cudaMalloc(&gpu_result, sizeof(unsigned long long)) != cudaSuccess) {
    abort();
  }

  if (cudaMemcpy(gpu_result, &result, sizeof(unsigned long long), cudaMemcpyHostToDevice) != cudaSuccess) {
    abort();
  }

  gpu_sum_kernel<<<grid, block>>>(rect, x, gpu_result);

  if (cudaMemcpy(&result, gpu_result, sizeof(unsigned long long), cudaMemcpyDeviceToHost) != cudaSuccess) {
    abort();
  }

  int64_t sum = result;
  // printf("gpu sum is %" PRId64 "\n", sum);
  return sum;
}
