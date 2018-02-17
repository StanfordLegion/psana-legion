#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <mpi.h>

#include "native_kernels.h"

#define CHECK(result) assert(!result)

int main(int argc, char **argv)
{
  CHECK(MPI_Init(&argc, &argv));

  int rank;
  CHECK(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
  int num_ranks;
  CHECK(MPI_Comm_size(MPI_COMM_WORLD, &num_ranks));

  long long buffer_size = 64; // MB
  long long rounds = 20;
  long long iterations = 20;
  for (int i = 1; i < argc; i++) {
    // Skip any legion runtime configuration parameters
    if (!strcmp(argv[i], "-size")) {
      buffer_size = atoll(argv[++i]);
      continue;
    }
    if (!strcmp(argv[i], "-rounds")) {
      rounds = atoll(argv[++i]);
      continue;
    }
    if (!strcmp(argv[i], "-iter")) {
      iterations = atoll(argv[++i]);
      continue;
    }
  }
  assert(buffer_size >= 0);
  assert(rounds >= 0);
  assert(iterations >= 0);

  long long buffer_size_bytes = buffer_size << 20; // MB -> bytes

  if (rank == 0) {
    printf("Kernel using %lld MB buffer for %lld rounds\n",
           buffer_size, rounds);
    printf("Running %lld iterations on %d ranks...\n",
           iterations, num_ranks);
  }

  CHECK(MPI_Barrier(MPI_COMM_WORLD));

  double start_ts = MPI_Wtime();

  for (int i = 0; i < iterations; i++) {
    memory_bound_kernel(buffer_size_bytes, rounds);
  }

  CHECK(MPI_Barrier(MPI_COMM_WORLD));

  double stop_ts = MPI_Wtime();

  double elapsed = stop_ts - start_ts;

  long long total_tasks = iterations*num_ranks;
  long long total_read = total_tasks*rounds*buffer_size;

  if (rank == 0) {
    printf("Elapsed time: %e seconds\n", elapsed);
    printf("Throughput: %e tasks/s\n", total_tasks/elapsed);
    printf("Time per task: %e seconds\n", elapsed/total_tasks);
    printf("Memory bandwidth achieved: %e GB/s\n", total_read/elapsed/(1<<10));
    printf("Total tasks: %lld\n", total_tasks);
    printf("Total data read: %lld MB\n", total_read);
  }

  CHECK(MPI_Finalize());
}
