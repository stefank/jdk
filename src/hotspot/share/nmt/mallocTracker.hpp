/*
 * Copyright (c) 2014, 2023, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2021, 2023 SAP SE. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_NMT_MALLOCTRACKER_HPP
#define SHARE_NMT_MALLOCTRACKER_HPP

#include "memory/allocation.hpp"
#include "nmt/mallocHeader.hpp"
#include "nmt/nmtCommon.hpp"
#include "runtime/atomic.hpp"
#include "runtime/threadCritical.hpp"
#include "utilities/nativeCallStack.hpp"

class outputStream;
struct malloclimit;

/*
 * This counter class counts memory allocation and deallocation,
 * records total memory allocation size and number of allocations.
 * The counters are updated atomically.
 */
class MemoryCounter {
 private:
  volatile size_t   _count;
  volatile Bytes   _size;

  // Peak size and count. Note: Peak count is the count at the point
  // peak size was reached, not the absolute highest peak count.
  volatile size_t _peak_count;
  volatile Bytes _peak_size;
  void update_peak(Bytes size, size_t cnt);

 public:
  MemoryCounter() : _count(0), _size(Bytes(0)), _peak_count(0), _peak_size(Bytes(0)) {}

  inline void set_size_and_count(Bytes size, size_t count) {
    _size = size;
    _count = count;
    update_peak(size, count);
  }

  inline void allocate(Bytes sz) {
    size_t cnt = Atomic::add(&_count, size_t(1), memory_order_relaxed);
    if (sz > Bytes(0)) {
      Bytes sum = in_Bytes(Atomic::add((volatile size_t*)&_size, untype(sz), memory_order_relaxed));
      update_peak(sum, cnt);
    }
  }

  inline void deallocate(Bytes sz) {
    assert(count() > 0, "Nothing allocated yet");
    assert(size() >= sz, "deallocation > allocated");
    Atomic::dec(&_count, memory_order_relaxed);
    if (sz > Bytes(0)) {
      Atomic::sub((volatile size_t*)&_size, untype(sz), memory_order_relaxed);
    }
  }

  inline void resize(ssize_t sz) {
    if (sz != 0) {
      assert(sz >= 0 || size() >= in_Bytes(size_t(-sz)), "Must be");
      Bytes sum = in_Bytes(Atomic::add((volatile size_t*)&_size, size_t(sz), memory_order_relaxed));
      update_peak(sum, _count);
    }
  }

  inline size_t count() const { return Atomic::load(&_count); }
  inline Bytes size()   const { return in_Bytes(Atomic::load((volatile size_t*)&_size));  }

  inline size_t peak_count() const {
    return Atomic::load(&_peak_count);
  }

  inline Bytes peak_size() const {
    return in_Bytes(Atomic::load((volatile size_t*)&_peak_size));
  }
};

/*
 * Malloc memory used by a particular subsystem.
 * It includes the memory acquired through os::malloc()
 * call and arena's backing memory.
 */
class MallocMemory {
 private:
  MemoryCounter _malloc;
  MemoryCounter _arena;

 public:
  MallocMemory() { }

  inline void record_malloc(Bytes sz) {
    _malloc.allocate(sz);
  }

  inline void record_free(Bytes sz) {
    _malloc.deallocate(sz);
  }

  inline void record_new_arena() {
    _arena.allocate(Bytes(0));
  }

  inline void record_arena_free() {
    _arena.deallocate(Bytes(0));
  }

  inline void record_arena_size_change(ssize_t sz) {
    _arena.resize(sz);
  }

  inline Bytes malloc_size()  const { return _malloc.size(); }
  inline Bytes malloc_peak_size()  const { return _malloc.peak_size(); }
  inline size_t malloc_count() const { return _malloc.count();}
  inline Bytes arena_size()   const { return _arena.size();  }
  inline Bytes arena_peak_size()  const { return _arena.peak_size(); }
  inline size_t arena_count()  const { return _arena.count(); }

  const MemoryCounter* malloc_counter() const { return &_malloc; }
  const MemoryCounter* arena_counter()  const { return &_arena;  }
};

class MallocMemorySummary;

// A snapshot of malloc'd memory, includes malloc memory
// usage by types and memory used by tracking itself.
class MallocMemorySnapshot {
  friend class MallocMemorySummary;

 private:
  MallocMemory      _malloc[mt_number_of_types];
  MemoryCounter     _all_mallocs;


 public:
  inline MallocMemory* by_type(MEMFLAGS flags) {
    int index = NMTUtil::flag_to_index(flags);
    return &_malloc[index];
  }

  inline const MallocMemory* by_type(MEMFLAGS flags) const {
    int index = NMTUtil::flag_to_index(flags);
    return &_malloc[index];
  }

  inline Bytes malloc_overhead() const {
    return _all_mallocs.count() * in_Bytes(sizeof(MallocHeader));
  }

  // Total malloc invocation count
  size_t total_count() const {
    return _all_mallocs.count();
  }

  // Total malloc'd memory amount
  Bytes total() const {
    return _all_mallocs.size() + malloc_overhead() + total_arena();
  }

  // Total malloc'd memory used by arenas
  Bytes total_arena() const;

  void copy_to(MallocMemorySnapshot* s);

  // Make adjustment by subtracting chunks used by arenas
  // from total chunks to get total free chunk size
  void make_adjustment();
};

/*
 * This class is for collecting malloc statistics at summary level
 */
class MallocMemorySummary : AllStatic {
 private:
  // Reserve memory for placement of MallocMemorySnapshot object
  static MallocMemorySnapshot _snapshot;
  static bool _have_limits;

  // Called when a total limit break was detected.
  // Will return true if the limit was handled, false if it was ignored.
  static bool total_limit_reached(Bytes s, Bytes so_far, const malloclimit* limit);

  // Called when a total limit break was detected.
  // Will return true if the limit was handled, false if it was ignored.
  static bool category_limit_reached(MEMFLAGS f, Bytes s, Bytes so_far, const malloclimit* limit);

 public:
   static void initialize();

   static inline void record_malloc(Bytes size, MEMFLAGS flag) {
     as_snapshot()->by_type(flag)->record_malloc(size);
     as_snapshot()->_all_mallocs.allocate(size);
   }

   static inline void record_free(Bytes size, MEMFLAGS flag) {
     as_snapshot()->by_type(flag)->record_free(size);
     as_snapshot()->_all_mallocs.deallocate(size);
   }

   static inline void record_new_arena(MEMFLAGS flag) {
     as_snapshot()->by_type(flag)->record_new_arena();
   }

   static inline void record_arena_free(MEMFLAGS flag) {
     as_snapshot()->by_type(flag)->record_arena_free();
   }

   static inline void record_arena_size_change(ssize_t size, MEMFLAGS flag) {
     as_snapshot()->by_type(flag)->record_arena_size_change(size);
   }

   static void snapshot(MallocMemorySnapshot* s) {
     as_snapshot()->copy_to(s);
     s->make_adjustment();
   }

   // The memory used by malloc tracking headers
   static inline Bytes tracking_overhead() {
     return as_snapshot()->malloc_overhead();
   }

  static MallocMemorySnapshot* as_snapshot() {
    return &_snapshot;
  }

  // MallocLimit: returns true if allocating s bytes on f would trigger
  // either global or the category limit
  static inline bool check_exceeds_limit(Bytes s, MEMFLAGS f);

};

// Main class called from MemTracker to track malloc activities
class MallocTracker : AllStatic {
 public:
  // Initialize malloc tracker for specific tracking level
  static bool initialize(NMT_TrackingLevel level);

  // The overhead that is incurred by switching on NMT (we need, per malloc allocation,
  // space for header and 16-bit footer)
  static const size_t overhead_per_malloc = sizeof(MallocHeader) + sizeof(uint16_t);

  // Parameter name convention:
  // memblock :   the beginning address for user data
  // malloc_base: the beginning address that includes malloc tracking header
  //
  // The relationship:
  // memblock = (char*)malloc_base + sizeof(nmt header)
  //

  // Record  malloc on specified memory block
  static void* record_malloc(void* malloc_base, Bytes size, MEMFLAGS flags,
    const NativeCallStack& stack);

  // Given a block returned by os::malloc() or os::realloc():
  // deaccount block from NMT, mark its header as dead and return pointer to header.
  static void* record_free_block(void* memblock);
  // Given the free info from a block, de-account block from NMT.
  static void deaccount(MallocHeader::FreeInfo free_info);

  static inline void record_new_arena(MEMFLAGS flags) {
    MallocMemorySummary::record_new_arena(flags);
  }

  static inline void record_arena_free(MEMFLAGS flags) {
    MallocMemorySummary::record_arena_free(flags);
  }

  static inline void record_arena_size_change(ssize_t size, MEMFLAGS flags) {
    MallocMemorySummary::record_arena_size_change(size, flags);
  }

  // MallocLimt: Given an allocation size s, check if mallocing this much
  // under category f would hit either the global limit or the limit for category f.
  static inline bool check_exceeds_limit(Bytes s, MEMFLAGS f);

  // Given a pointer, look for the containing malloc block.
  // Print the block. Note that since there is very low risk of memory looking
  // accidentally like a valid malloc block header (canaries and all) this is not
  // totally failproof. Only use this during debugging or when you can afford
  // signals popping up, e.g. when writing an hs_err file.
  static bool print_pointer_information(const void* p, outputStream* st);

  static inline MallocHeader* malloc_header(void *memblock) {
    assert(memblock != nullptr, "null pointer");
    return (MallocHeader*)memblock -1;
  }
  static inline const MallocHeader* malloc_header(const void *memblock) {
    assert(memblock != nullptr, "null pointer");
    return (const MallocHeader*)memblock -1;
  }
};

#endif // SHARE_NMT_MALLOCTRACKER_HPP
