#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include <omp.h>

long long get_time_ns() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  long long t = (1000000000LL * ts.tv_sec) + ts.tv_nsec;
  return t;
}

int main(int argc, const char **argv) {
  if (argc != 4) {
    printf("Usage: %s <filename> <nshards> <chunksize>\n", argv[0]);
    return -1;
  }

  const char *filename = argv[1];
  size_t nshards = atoll(argv[2]);
  assert(nshards != 0);
  size_t chunksize = atoll(argv[3]);
  assert(chunksize != 0);

  printf("Parameters:\n");
  printf("  Filename: %s\n", filename);
  printf("  Number of shards: %lu\n", nshards);
  printf("  Chunk size: %lu\n", chunksize);
  printf("\n");

  int fd = open(filename, O_RDONLY | O_DIRECT);
  assert(fd >= 0);

  long long start_ns = get_time_ns();

  omp_set_num_threads(nshards);

  size_t total_bytes = 0;
  #pragma omp parallel for reduction(+:total_bytes)
  for (size_t shard = 0; shard < nshards; shard++) {
    printf("Shard %lu running on thread %d\n", shard, omp_get_thread_num());

    void *buffer = 0;
    const size_t alignment = 4096;
    int err = posix_memalign(&buffer, alignment, chunksize);
    assert(!err);
    assert(buffer);

    size_t offset = shard * chunksize;
    while (true) {
      ssize_t actual = pread(fd, buffer, chunksize, offset);
      if (actual > 0) {
        // good, keep going
        offset += nshards * chunksize; // advance to next chunk
        total_bytes += actual; // track total bytes read
      } else if (actual == 0) {
        break; // end of file
      } else if (actual < 0) {
        assert(false); // error
      }
    }
    free(buffer);
  }

  long long stop_ns = get_time_ns();

  double elapsed_s = (stop_ns - start_ns)/1e9;

  printf("\n");
  printf("Elapsed time: %e seconds\n", elapsed_s);
  printf("Total bytes: %.1f GB (1 GB == 10^9 bytes)\n", total_bytes/1e9);
  printf("Bandwidth: %.1f GB/s\n", total_bytes/elapsed_s/1e9);

  close(fd);
}
