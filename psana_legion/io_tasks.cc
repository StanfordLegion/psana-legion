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

#include "io_tasks.h"

#ifdef REALM_USE_SUBPROCESSES

#define PSANA_USE_LEGION
#ifdef PSANA_USE_LEGION

#include <fcntl.h>
#include <pthread.h>
#include <legion.h>
#include <legion/legion_c.h>
#include <legion/legion_c_util.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef REALM_USE_SUBPROCESSES
#include "realm/custom_malloc.h"
#include "realm/runtime_impl.h"
#define INSTALL_REALM_ALLOCATOR Realm::ScopedAllocatorPush sap(Realm::RuntimeImpl::realm_allocator)
#else
#define INSTALL_REALM_ALLOCATOR do {} while (0)
#endif

#ifdef REALM_USE_SUBPROCESSES
#include "pdsdata_includes.h"
#endif

#endif // PSANA_USE_LEGION

class PsanaRandomAccessXtcReader {
public:
  PsanaRandomAccessXtcReader() {}

  static bool has(const std::string& filename) {
    // WARNING: Only call this while holding _fd_mutex
    return _fd.count(filename);
  }

  static int ensure(const std::string& filename) {
#ifdef PSANA_USE_LEGION
    if (pthread_mutex_lock(&_fd_mutex)) {
      assert(false && "pthread_mutex_lock failed\n");
    }
#endif

    if (has(filename)) {
      int result = _fd[filename];

#ifdef PSANA_USE_LEGION
      if (pthread_mutex_unlock(&_fd_mutex)) {
        assert(false && "pthread_mutex_unlock failed\n");
      }
#endif

      return result;
    }

    int fd = ::open(filename.c_str(), O_RDONLY | O_LARGEFILE);
    if (fd==-1) {
      abort();
      // MsgLog(logger, fatal,
      //        "File " << filename.c_str() << " not found");
    }
    _fd[filename] = fd;

#ifdef PSANA_USE_LEGION
    if (pthread_mutex_unlock(&_fd_mutex)) {
      assert(false && "pthread_mutex_unlock failed\n");
    }
#endif

    return fd;
  }

  ~PsanaRandomAccessXtcReader() {
    // FIXME: Not safe to close fds in the destructor since the memory is now static
    // for  (std::map<std::string, int>::const_iterator it = _fd.begin(); it!= _fd.end(); it++)
    //   ::close(it->second);
  }

#ifdef PSANA_USE_LEGION
  static bool jump_internal(const std::string& filename, int64_t offset, wrapper::Pds::Dgram* dg) {
    int fd = ensure(filename);
    if (::pread(fd, dg, sizeof(wrapper::Pds::Dgram), offset)==0) {
      return false;
    } else {
      if (dg->xtc.sizeofPayload()>MaxDgramSize) {
        abort();
        // MsgLog(logger, fatal, "Datagram size exceeds sanity check. Size: " << dg->xtc.sizeofPayload() << " Limit: " << MaxDgramSize);
      }
      ::pread(fd, dg->xtc.payload(), dg->xtc.sizeofPayload(), offset+sizeof(wrapper::Pds::Dgram));
      return true;
    }
  }

  static bool jump_task(const Legion::Task *task,
            const std::vector<Legion::PhysicalRegion> &regions,
            Legion::Context ctx, Legion::HighLevelRuntime *runtime) {
    // Unpack arguments.
    assert(task->arglen >= sizeof(Args));
    size_t count;
    std::vector<int64_t> offsets;
    std::vector<std::string> filenames;
    {
      const char *current = (const char *)task->args;

      size_t total_bytes = *(size_t *)current; current += sizeof(size_t);
      count = *(size_t *)current; current += sizeof(size_t);

      for (size_t i = 0; i < count; i++) {
        offsets.push_back(*(int64_t *)current); current += sizeof(int64_t);
      }

      std::vector<size_t> filename_sizes;
      for (size_t i = 0; i < count; i++) {
        filename_sizes.push_back(*(size_t *)current); current += sizeof(size_t);
      }

      for (size_t i = 0; i < count; i++) {
        filenames.push_back(std::string(current, filename_sizes[i]));
        current += filename_sizes[i];
      }

      assert(current == ((const char *)task->args) + total_bytes);
    }

    // Fetch destination pointer out of region argument.
    LegionRuntime::Arrays::Rect<1> rect;
    {
      INSTALL_REALM_ALLOCATOR;
      rect = runtime->get_index_space_domain(
        regions[0].get_logical_region().get_index_space()).get_rect<1>();
    }
    LegionRuntime::Arrays::Rect<1> subrect;
    LegionRuntime::Accessor::ByteOffset stride;


    bool ok = true;
    for (size_t i = 0; i < count; i++) {
      void *base_ptr;
      {
        INSTALL_REALM_ALLOCATOR;
        LegionRuntime::Accessor::RegionAccessor<LegionRuntime::Accessor::AccessorType::Generic> accessor =
          regions[0].get_field_accessor(fid_base+i);
        base_ptr = accessor.raw_rect_ptr<1>(rect, subrect, &stride);
      }
      assert(base_ptr);
      assert(subrect == rect);
      assert(rect.lo == LegionRuntime::Arrays::Point<1>::ZEROES());
      assert(stride.offset == 1);

      // Call base jump.
      // FIXME: Maybe not the right error behavior
      ok = ok && jump_internal(filenames[i], offsets[i], (wrapper::Pds::Dgram *)base_ptr);
    }
    return ok;
  }

  static Legion::TaskID preregister_jump_task() {
    INSTALL_REALM_ALLOCATOR;
    static const char * const task_name = "jump";
    Legion::TaskVariantRegistrar registrar(task_id, task_name, false /* global */);
    registrar.add_constraint(Legion::ProcessorConstraint(Legion::Processor::IO_PROC));
    Legion::Runtime::preregister_task_variant<bool, jump_task>(registrar, task_name);
    return task_id;
  }

  static const Legion::TaskID task_id = 501976; // chosen by fair dice roll
  static const Legion::FieldID fid_base = 100;
  struct Args {
    size_t total_bytes;
    size_t count;
    // std::vector<int64_t> offsets;
    // std::vector<size_t> filename_sizes;
    // std::vector<std::string content> filenames;
  };
#endif

private:
  enum {MaxDgramSize=0x1000000};
  static std::map<std::string, int> _fd;
#ifdef PSANA_USE_LEGION
  static pthread_mutex_t _fd_mutex;
#endif
};

void preregister_io_tasks() {
  PsanaRandomAccessXtcReader::preregister_jump_task();
}

std::map<std::string, int> PsanaRandomAccessXtcReader::_fd;
#ifdef PSANA_USE_LEGION
pthread_mutex_t PsanaRandomAccessXtcReader::_fd_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

#endif // REALM_USE_SUBPROCESSES
