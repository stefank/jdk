/*
 * Copyright (c) 2015, 2025, Oracle and/or its affiliates. All rights reserved.
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
 */

#include "gc/shared/gcLogPrecious.hpp"
#include "gc/shared/suspendibleThreadSet.hpp"
#include "gc/z/zAddress.hpp"
#include "gc/z/zAllocationFlags.hpp"
#include "gc/z/zArray.inline.hpp"
#include "gc/z/zDriver.hpp"
#include "gc/z/zFuture.inline.hpp"
#include "gc/z/zGeneration.inline.hpp"
#include "gc/z/zGenerationId.hpp"
#include "gc/z/zGlobals.hpp"
#include "gc/z/zLargePages.inline.hpp"
#include "gc/z/zLock.inline.hpp"
#include "gc/z/zMappedCache.hpp"
#include "gc/z/zMemory.inline.hpp"
#include "gc/z/zNUMA.inline.hpp"
#include "gc/z/zPage.inline.hpp"
#include "gc/z/zPageAge.hpp"
#include "gc/z/zPageAllocator.inline.hpp"
#include "gc/z/zPageType.hpp"
#include "gc/z/zSafeDelete.inline.hpp"
#include "gc/z/zStat.hpp"
#include "gc/z/zTask.hpp"
#include "gc/z/zUncommitter.hpp"
#include "gc/z/zUtils.inline.hpp"
#include "gc/z/zValue.inline.hpp"
#include "gc/z/zVirtualMemoryManager.hpp"
#include "gc/z/zWorkers.hpp"
#include "jfr/jfrEvents.hpp"
#include "logging/log.hpp"
#include "memory/allocation.hpp"
#include "nmt/memTag.hpp"
#include "runtime/globals.hpp"
#include "runtime/init.hpp"
#include "runtime/java.hpp"
#include "runtime/os.hpp"
#include "utilities/align.hpp"
#include "utilities/debug.hpp"
#include "utilities/globalDefinitions.hpp"

#include <cmath>

class ZMemoryAllocation;

static const ZStatCounter       ZCounterMutatorAllocationRate("Memory", "Allocation Rate", ZStatUnitBytesPerSecond);
static const ZStatCounter       ZCounterDefragment("Memory", "Defragment", ZStatUnitOpsPerSecond);
static const ZStatCriticalPhase ZCriticalPhaseAllocationStall("Allocation Stall");

static void sort_zbacking_index_array(zbacking_index* array, size_t count) {
  ZUtils::sort(array, count, [](const zbacking_index* e1, const zbacking_index* e2) {
    return *e1 < *e2 ? -1 : 1;
  });
}

static void check_numa_mismatch(const ZVirtualMemory& vmem, uint32_t desired_id) {
  if (ZNUMA::is_enabled()) {
    // Check if memory ended up on desired NUMA node or not
    const uint32_t actual_id = ZNUMA::memory_id(untype(ZOffset::address(vmem.start())));
    if (actual_id != desired_id) {
      log_debug(gc, heap)("NUMA Mismatch: desired %d, actual %d", desired_id, actual_id);
    }
  }
}

class ZSegmentStash {
private:
  ZGranuleMap<zbacking_index>* const _physical_mappings;
  ZArray<zbacking_index>             _stash;

  void sort_stashed_segments() {
    sort_zbacking_index_array(_stash.adr_at(0), (size_t)_stash.length());
  }

  void copy_to_stash(int index, const ZVirtualMemory& vmem) {
    zbacking_index* const dest = _stash.adr_at(index);
    const zbacking_index* const src = _physical_mappings->addr(vmem.start());
    const size_t num_granules = vmem.size_in_granules();

    // Copy to stash
    ZUtils::copy_disjoint(dest, src, num_granules);
  }

  void copy_from_stash(int index, const ZVirtualMemory& vmem) {
    zbacking_index* const dest = _physical_mappings->addr(vmem.start());
    const zbacking_index* const src = _stash.adr_at(index);
    const size_t num_granules = vmem.size_in_granules();

    // Copy from stash
    ZUtils::copy_disjoint(dest, src, num_granules);
  }

public:
  ZSegmentStash(ZGranuleMap<zbacking_index>* physical_mappings, int num_granules)
    : _physical_mappings(physical_mappings),
      _stash(num_granules, num_granules, zbacking_index::zero) {}

  void stash(const ZVirtualMemory& vmem) {
    copy_to_stash(0, vmem);
    sort_stashed_segments();
  }

  void stash(ZArray<ZVirtualMemory>* vmems) {
    int stash_index = 0;
    ZArrayIterator<ZVirtualMemory> iter(vmems);
    for (ZVirtualMemory vmem; iter.next(&vmem);) {
      const size_t num_granules = vmem.size_in_granules();
      copy_to_stash(stash_index, vmem);
      stash_index += (int)num_granules;
    }
    sort_stashed_segments();
  }

  void pop(ZArray<ZVirtualMemory>* vmems, size_t num_vmems) {
    int stash_index = 0;
    const int pop_start_index = vmems->length() - (int)num_vmems;
    ZArrayIterator<ZVirtualMemory> iter(vmems, pop_start_index);
    for (ZVirtualMemory vmem; iter.next(&vmem);) {
      const size_t num_granules = vmem.size_in_granules();
      const size_t granules_left = _stash.length() - stash_index;

      // If we run out of segments in the stash, we finish early
      if (num_granules >= granules_left) {
        const ZVirtualMemory truncated_vmem(vmem.start(), granules_left * ZGranuleSize);
        copy_from_stash(stash_index, truncated_vmem);
        return;
      }

      copy_from_stash(stash_index, vmem);
      stash_index += (int)num_granules;
    }
  }

  void pop_all(ZArray<ZVirtualMemory>* vmems) {
    pop(vmems, vmems->length());
  }

  void pop_all(ZVirtualMemory vmem) {
    const size_t granules_left = _stash.length();
    const ZVirtualMemory to_pop = vmem.first_part(granules_left * ZGranuleSize);

    copy_from_stash(0, vmem);
  }
};

class ZMemoryAllocation : public CHeapObj<mtGC> {
private:
  const size_t           _size;
  uint32_t               _numa_id;
  ZVirtualMemory         _satisfied_from_cache_vmem;
  ZArray<ZVirtualMemory> _partial_vmems;
  int                    _num_harvested;
  size_t                 _harvested;
  size_t                 _increased_capacity;
  size_t                 _committed_capacity;
  bool                   _commit_failed;

  explicit ZMemoryAllocation(const ZMemoryAllocation& other)
    : ZMemoryAllocation(other._size) {
      // Transfer the numa id
    set_numa_id(other._numa_id);

    // Reserve space for the partial vmems
    _partial_vmems.reserve(other._partial_vmems.length() + (other._satisfied_from_cache_vmem.is_null() ? 1 : 0));

    // Transfer the claimed capacity
    transfer_claimed_capacity(other);
  }

  ZMemoryAllocation(const ZMemoryAllocation& a1, const ZMemoryAllocation& a2)
    : ZMemoryAllocation(a1._size + a2._size) {
    // Transfer the numa id
    assert(a1._numa_id == a2._numa_id, "only merge with same numa_id");
    set_numa_id(a1._numa_id);

    // Reserve space for the partial vmems
    const size_t num_vmems_a1 = a1._partial_vmems.length() + (a1._satisfied_from_cache_vmem.is_null() ? 1 : 0);
    const size_t num_vmems_a2 = a2._partial_vmems.length() + (a2._satisfied_from_cache_vmem.is_null() ? 1 : 0);
    const int total_vmems = (int)(num_vmems_a1 + num_vmems_a2);
    _partial_vmems.reserve(total_vmems);

    // Transfer the claimed capacity
    transfer_claimed_capacity(a1);
    transfer_claimed_capacity(a2);
  }

  void transfer_claimed_capacity(const ZMemoryAllocation& from) {
    assert(from._committed_capacity == 0, "Unexpected value %zu", from._committed_capacity);
    assert(!from._commit_failed, "Unexpected value");
    // Transfer increased capacity
    _increased_capacity += from._increased_capacity;

    // Transfer satisfying vmem or partial mappings
    const ZVirtualMemory vmem = from._satisfied_from_cache_vmem;
    if (!vmem.is_null()) {
      assert(_partial_vmems.is_empty(), "Must either have result or partial vmems");
      _partial_vmems.push(vmem);
      _num_harvested += 1;
      _harvested += vmem.size();
    } else {
      _partial_vmems.appendAll(&from._partial_vmems);
      _num_harvested += from._num_harvested;
      _harvested += from._harvested;
    }
  }

public:
  explicit ZMemoryAllocation(size_t size)
    : _size(size),
      _numa_id(-1),
      _satisfied_from_cache_vmem(),
      _partial_vmems(0),
      _num_harvested(0),
      _harvested(0),
      _increased_capacity(0),
      _committed_capacity(0),
      _commit_failed(false) {}

  ZVirtualMemory satisfied_from_cache_vmem() const {
    return _satisfied_from_cache_vmem;
  }

  void set_satisfied_from_cache_vmem(ZVirtualMemory vmem) {
    precond(_satisfied_from_cache_vmem.is_null());
    precond(vmem.size() == size());
    precond(_partial_vmems.is_empty());

    _satisfied_from_cache_vmem = vmem;
  }

  void reset_for_retry() {
    _partial_vmems.clear();
    _harvested = 0;
    _increased_capacity = 0;
    _committed_capacity = 0;
  }

  size_t size() const {
    return _size;
  }

  int num_harvested() const {
    return _num_harvested;
  }

  size_t harvested() const {
    return _harvested;
  }

  void set_harvested(int num_harvested, size_t harvested) {
    _num_harvested = num_harvested;
    _harvested = harvested;
  }

  size_t increased_capacity() const {
    return _increased_capacity;
  }

  void set_increased_capacity(size_t increased_capacity) {
    _increased_capacity = increased_capacity;
  }

  size_t committed_capacity() const {
    return _committed_capacity;
  }

  void set_committed_capacity(size_t committed_capacity) {
    assert(_committed_capacity == 0, "Should this only be set once?");
    _committed_capacity = committed_capacity;
  }

  uint32_t numa_id() const {
    assert(_numa_id != (uint32_t)-1, "Should have been initialized");
    return _numa_id;
  }

  void set_numa_id(uint32_t numa_id) {
    assert(_numa_id == (uint32_t)-1, "Should be initialized only once");
    _numa_id = numa_id;
  }

  bool commit_failed() const {
    return _commit_failed;
  }

  void set_commit_failed() {
    _commit_failed = true;
  }

  ZArray<ZVirtualMemory>* partial_vmems() {
    return &_partial_vmems;
  }

  const ZArray<ZVirtualMemory>* partial_vmems() const {
    return &_partial_vmems;
  }

  static void destroy(ZMemoryAllocation* allocation) {
    delete allocation;
  }

  static void merge(const ZMemoryAllocation& allocation, ZMemoryAllocation** merge_location) {
    ZMemoryAllocation* const other_allocation = *merge_location;
    if (other_allocation == nullptr) {
      // First allocation, allocate new node
      *merge_location = new ZMemoryAllocation(allocation);
    } else {
      // Merge with other allocation
      *merge_location = new ZMemoryAllocation(allocation, *other_allocation);

      // Delete old allocation
      delete other_allocation;
    }
  }
};

class ZSingleNodeAllocation {
private:
  ZMemoryAllocation _allocation;

public:
  ZSingleNodeAllocation(size_t size)
    : _allocation(size) {}

  size_t size() const {
    return _allocation.size();
  }

  ZMemoryAllocation* allocation() {
    return &_allocation;
  }

  const ZMemoryAllocation* allocation() const {
    return &_allocation;
  }

  void reset_for_retry() {
    _allocation.reset_for_retry();
  }
};

class ZMultiNodeAllocation : public StackObj {
private:
  const size_t               _size;
  ZArray<ZMemoryAllocation*> _allocations;
  ZVirtualMemory             _final_vmem;
  uint32_t                   _final_vmem_numa_id;

public:
  ZMultiNodeAllocation(size_t size)
    : _size(size),
      _allocations(0),
      _final_vmem(),
      _final_vmem_numa_id(-1) {}

  ~ZMultiNodeAllocation() {
    for (ZMemoryAllocation* allocation : _allocations) {
      ZMemoryAllocation::destroy(allocation);
    }
  }

  void initialize() {
    precond(_allocations.is_empty());

    // The multi node allocation creates at most one allocation per node.
    const int length = (int)ZNUMA::count();

    _allocations.reserve(length);
  }

  void reset_for_retry() {
    for (ZMemoryAllocation* allocation : _allocations) {
      delete allocation;
    }
    _allocations.clear();

    _final_vmem = {};
    _final_vmem_numa_id = -1;
  }

  size_t size() const {
    return _size;
  }

  ZArray<ZMemoryAllocation*>* allocations() {
    return &_allocations;
  }

  const ZArray<ZMemoryAllocation*>* allocations() const {
    return &_allocations;
  }

  void register_allocation(const ZMemoryAllocation& allocation) {
    ZMemoryAllocation** const slot = allocation_slot(allocation.numa_id());

    ZMemoryAllocation::merge(allocation, slot);
  }

  ZMemoryAllocation** allocation_slot(uint32_t numa_id) {
    // Try to find an existing allocation for numa_id
    for (int i = 0; i < _allocations.length(); ++i) {
      ZMemoryAllocation** const slot_addr = _allocations.adr_at(i);
      ZMemoryAllocation* const allocation = *slot_addr;
      if (allocation->numa_id() == numa_id) {
        // Found an existing slot
        return slot_addr;
      }
    }

    // Push an empty slot for the numa_id
    _allocations.push(nullptr);

    // Return the address of the slot
    return &_allocations.last();
  }

  int sum_num_harvested_vmems() const {
    int total = 0;

    for (const ZMemoryAllocation* allocation : _allocations) {
      total += allocation->num_harvested();
    }

    return total;
  }

  size_t sum_harvested() const {
    size_t total = 0;

    for (const ZMemoryAllocation* allocation : _allocations) {
      total += allocation->harvested();
    }

    return total;
  }

  size_t sum_committed_increased_capacity() const {
    size_t total = 0;

    for (const ZMemoryAllocation* allocation : _allocations) {
      total += allocation->committed_capacity();
    }

    return total;
  }

  void set_final_vmem(const ZVirtualMemory& vmem, uint32_t numa_id) {
    precond(_final_vmem.is_null());
    precond(_final_vmem_numa_id == (uint32_t)-1);

    _final_vmem = vmem;
    _final_vmem_numa_id = numa_id;
  }

  ZVirtualMemory final_vmem() const {
    return _final_vmem;
  }

  ZVirtualMemory pop_final_vmem() {
    const ZVirtualMemory vmem = _final_vmem;
    _final_vmem = {};
    return vmem;
  }
};

struct ZPageAllocationStats {
  int    _num_harvested_vmems;
  size_t _total_harvested;
  size_t _total_committed_capacity;

  ZPageAllocationStats(int num_harvested_vmems, size_t total_harvested, size_t total_committed_capacity)
    : _num_harvested_vmems(num_harvested_vmems),
      _total_harvested(total_harvested),
      _total_committed_capacity(total_committed_capacity) {}
};

class ZPageAllocation : public StackObj {
  friend class ZList<ZPageAllocation>;

private:
  const ZPageType            _type;
  const size_t               _size;
  const ZAllocationFlags     _flags;
  const uint32_t             _young_seqnum;
  const uint32_t             _old_seqnum;
  const uint32_t             _initiating_numa_id;
  bool                       _is_multi_node;
  ZSingleNodeAllocation      _single_node_allocation;
  ZMultiNodeAllocation       _multi_node_allocation;
  ZListNode<ZPageAllocation> _node;
  ZFuture<bool>              _stall_result;

public:
  ZPageAllocation(ZPageType type, size_t size, ZAllocationFlags flags)
    : _type(type),
      _size(size),
      _flags(flags),
      _young_seqnum(ZGeneration::young()->seqnum()),
      _old_seqnum(ZGeneration::old()->seqnum()),
      _initiating_numa_id(ZNUMA::id()),
      _is_multi_node(false),
      _single_node_allocation(size),
      _multi_node_allocation(size),
      _node(),
      _stall_result() {}

  void reset_for_retry() {
    _is_multi_node = false;
    _single_node_allocation.reset_for_retry();
    _multi_node_allocation.reset_for_retry();
  }

  ZPageType type() const {
    return _type;
  }

  size_t size() const {
    return _size;
  }

  ZAllocationFlags flags() const {
    return _flags;
  }

  uint32_t young_seqnum() const {
    return _young_seqnum;
  }

  uint32_t old_seqnum() const {
    return _old_seqnum;
  }

  uint32_t initiating_numa_id() const {
    return _initiating_numa_id;
  }

  bool is_multi_node() const {
    return _is_multi_node;
  }

  void initiate_multi_node_allocation() {
    assert(!_is_multi_node, "Reinitialization?");
    _is_multi_node = true;
    _multi_node_allocation.initialize();
  }

  ZMultiNodeAllocation* multi_node_allocation() {
    assert(_is_multi_node, "multi node allocation must be initiated");

    return &_multi_node_allocation;
  }

  const ZMultiNodeAllocation* multi_node_allocation() const {
    assert(_is_multi_node, "multi node allocation must be initiated");

    return &_multi_node_allocation;
  }

  ZSingleNodeAllocation* single_node_allocation() {
    assert(!_is_multi_node, "multi node allocation must not have been initiated");

    return &_single_node_allocation;
  }

  const ZSingleNodeAllocation* single_node_allocation() const {
    assert(!_is_multi_node, "multi node allocation must not have been initiated");

    return &_single_node_allocation;
  }

  ZVirtualMemory satisfied_from_cache_vmem() const {
    precond(!_is_multi_node);

    const ZMemoryAllocation* const allocation = _single_node_allocation.allocation();

    return allocation->satisfied_from_cache_vmem();
  }

  bool wait() {
    return _stall_result.get();
  }

  void satisfy(bool result) {
    _stall_result.set(result);
  }

  bool gc_relocation() const {
    return _flags.gc_relocation();
  }

  ZPageAllocationStats stats() const {
    if (_is_multi_node) {
      return ZPageAllocationStats(
          _multi_node_allocation.sum_num_harvested_vmems(),
          _multi_node_allocation.sum_harvested(),
          _multi_node_allocation.sum_committed_increased_capacity());
    } else {
      return ZPageAllocationStats(
          _single_node_allocation.allocation()->num_harvested(),
          _single_node_allocation.allocation()->harvested(),
          _single_node_allocation.allocation()->committed_capacity());
    }
  }
};

const ZVirtualMemoryManager& ZAllocNode::virtual_memory_manager() const {
  return _page_allocator->_virtual;
}

ZVirtualMemoryManager& ZAllocNode::virtual_memory_manager() {
  return _page_allocator->_virtual;
}

const ZPhysicalMemoryManager& ZAllocNode::physical_memory_manager() const {
  return _page_allocator->_physical;
}

ZPhysicalMemoryManager& ZAllocNode::physical_memory_manager() {
  return _page_allocator->_physical;
}

const ZGranuleMap<zbacking_index>& ZAllocNode::physical_mappings() const {
  return _page_allocator->_physical_mappings;
}

ZGranuleMap<zbacking_index>& ZAllocNode::physical_mappings() {
  return _page_allocator->_physical_mappings;
}

const zbacking_index* ZAllocNode::physical_mappings_addr(const ZVirtualMemory& vmem) const {
  const ZGranuleMap<zbacking_index>& mappings = physical_mappings();
  return mappings.addr(vmem.start());
}

zbacking_index* ZAllocNode::physical_mappings_addr(const ZVirtualMemory& vmem) {
  ZGranuleMap<zbacking_index>& mappings = physical_mappings();
  return mappings.addr(vmem.start());
}

void ZAllocNode::verify_virtual_memory_association(const ZVirtualMemory& vmem) const {
  const uint32_t vmem_numa_id = virtual_memory_manager().get_numa_id(vmem);
  assert(_numa_id == vmem_numa_id, "Virtual memory must be associated with the current node "
                                   "expected: %u, actual: %u", _numa_id, vmem_numa_id);
}

void ZAllocNode::verify_virtual_memory_association(const ZArray<ZVirtualMemory>* vmems) const {
  ZArrayIterator<ZVirtualMemory> iter(vmems);
  for (ZVirtualMemory vmem; iter.next(&vmem);) {
    verify_virtual_memory_association(vmem);
  }
}

void ZAllocNode::verify_memory_allocation_association(const ZMemoryAllocation* allocation) const {
  const uint32_t allocation_numa_id = allocation->numa_id();
  assert(_numa_id == allocation_numa_id, "Memory allocation must be associated with the current node "
                                         "expected: %u, actual: %u", _numa_id, allocation_numa_id);
}

ZAllocNode::ZAllocNode(uint32_t numa_id, ZPageAllocator* page_allocator)
  : _page_allocator(page_allocator),
    _cache(),
    _uncommitter(numa_id, this),
    _min_capacity(ZNUMA::calculate_share(numa_id, page_allocator->min_capacity())),
    _initial_capacity(ZNUMA::calculate_share(numa_id, page_allocator->initial_capacity())),
    _max_capacity(ZNUMA::calculate_share(numa_id, page_allocator->max_capacity())),
    _current_max_capacity(_max_capacity),
    _capacity(0),
    _claimed(0),
    _used(0),
    _used_generations{0,0},
    _collection_stats{{0, 0},{0, 0}},
    _last_commit(0.0),
    _last_uncommit(0.0),
    _to_uncommit(0),
    _numa_id(numa_id) {}

size_t ZAllocNode::available() const {
  return _current_max_capacity - _used - _claimed;
}

size_t ZAllocNode::increase_capacity(size_t size) {
  const size_t increased = MIN2(size, _current_max_capacity - _capacity);

  if (increased > 0) {
    // Update atomically since we have concurrent readers
    Atomic::add(&_capacity, increased);

    _last_commit = os::elapsedTime();
    _last_uncommit = 0;
    _cache.reset_min();
  }

  return increased;
}

void ZAllocNode::decrease_capacity(size_t size, bool set_max_capacity) {
  // Update state atomically since we have concurrent readers
  Atomic::sub(&_capacity, size);

  // Adjust current max capacity to avoid further attempts to increase capacity
  if (set_max_capacity) {
    Atomic::store(&_current_max_capacity, _capacity);
  }
}

void ZAllocNode::increase_used(size_t size) {
  // We don't track generation usage here because this page
  // could be allocated by a thread that satisfies a stalling
  // allocation. The stalled thread can wake up and potentially
  // realize that the page alloc should be undone. If the alloc
  // and the undo gets separated by a safepoint, the generation
  // statistics could se a decreasing used value between mark
  // start and mark end.

  // Update atomically since we have concurrent readers
  const size_t used = Atomic::add(&_used, size);

  // Update used high
  for (auto& stats : _collection_stats) {
    if (used > stats._used_high) {
      stats._used_high = used;
    }
  }
}

void ZAllocNode::decrease_used(size_t size) {
  // Update atomically since we have concurrent readers
  const size_t used = Atomic::sub(&_used, size);

  // Update used low
  for (auto& stats : _collection_stats) {
    if (used < stats._used_low) {
      stats._used_low = used;
    }
  }
}

void ZAllocNode::increase_used_generation(ZGenerationId id, size_t size) {
  // Update atomically since we have concurrent readers
  Atomic::add(&_used_generations[(int)id], size, memory_order_relaxed);
}

void ZAllocNode::decrease_used_generation(ZGenerationId id, size_t size) {
  // Update atomically since we have concurrent readers
  Atomic::sub(&_used_generations[(int)id], size, memory_order_relaxed);
}

void ZAllocNode::reset_statistics(ZGenerationId id) {
  _collection_stats[(int)id]._used_high = _used;
  _collection_stats[(int)id]._used_low = _used;
}

void ZAllocNode::claim_from_cache_or_increase_capacity(ZMemoryAllocation* allocation) {
  const size_t size = allocation->size();
  ZArray<ZVirtualMemory>* const out = allocation->partial_vmems();

  // We are guaranteed to succeed the claiming of capacity here
  assert(available() >= size, "Must be");

  // Associate the allocation with this alloc node.
  allocation->set_numa_id(_numa_id);

  // Try to allocate one contiguous vmem
  ZVirtualMemory vmem = _cache.remove_contiguous(size);
  if (!vmem.is_null()) {
    // Found a satisfying vmem in the cache
    allocation->set_satisfied_from_cache_vmem(vmem);

    // Done
    return;
  }

  // Try increase capacity
  const size_t increased_capacity = increase_capacity(size);

  allocation->set_increased_capacity(increased_capacity);

  if (increased_capacity == size) {
    // Capacity increase covered the entire request, done.
    return;
  }

  // Could not increase capacity enough to satisfy the allocation completely.
  // Try removing multiple vmems from the mapped cache. We only remove if
  // cache has enough remaining to cover the request.
  const size_t remaining = size - increased_capacity;
  const size_t harvested = _cache.remove_discontiguous(remaining, out);
  const int num_harvested = out->length();

  allocation->set_harvested(num_harvested, harvested);

  assert(harvested + increased_capacity == size, "Mismatch harvested: %zu increased_capacity: %zu size: %zu",
         harvested, increased_capacity, size);

  return;
}

bool ZAllocNode::claim_capacity(ZMemoryAllocation* allocation) {
  const size_t size = allocation->size();

  if (available() < size) {
    // Out of memory
    return false;
  }

  claim_from_cache_or_increase_capacity(allocation);

  // Updated used statistics
  increase_used(size);

  // Success
  return true;
}

void ZAllocNode::promote_used(size_t size) {
  decrease_used_generation(ZGenerationId::young, size);
  increase_used_generation(ZGenerationId::old, size);
}

ZMappedCache* ZAllocNode::cache() {
  return &_cache;
}

uint32_t ZAllocNode::numa_id() const {
  return _numa_id;
}

size_t ZAllocNode::uncommit(uint64_t* timeout) {
  ZArray<ZVirtualMemory> flushed_vmems;
  size_t flushed = 0;

  {
    // We need to join the suspendible thread set while manipulating capacity and
    // used, to make sure GC safepoints will have a consistent view.
    SuspendibleThreadSetJoiner sts_joiner;
    ZLocker<ZLock> locker(&_page_allocator->_lock);

    const double now = os::elapsedTime();
    const double time_since_last_commit = std::floor(now - _last_commit);
    const double time_since_last_uncommit = std::floor(now - _last_uncommit);

    if (time_since_last_commit < double(ZUncommitDelay)) {
      // We have committed within the delay, stop uncommitting.
      *timeout = uint64_t(double(ZUncommitDelay) - time_since_last_commit);
      return 0;
    }

    // We flush out and uncommit chunks at a time (~0.8% of the max capacity,
    // but at least one granule and at most 256M), in case demand for memory
    // increases while we are uncommitting.
    const size_t limit_upper_bound = MAX2(ZGranuleSize, align_down(256 * M / ZNUMA::count(), ZGranuleSize));
    const size_t limit = MIN2(align_up(_current_max_capacity >> 7, ZGranuleSize), limit_upper_bound);

    if (limit == 0) {
      // This may occur if the current max capacity for this node is 0

      // Set timeout to ZUncommitDelay
      *timeout = ZUncommitDelay;
      return 0;
    }

    if (time_since_last_uncommit < double(ZUncommitDelay)) {
      // We are in the uncommit phase
      const size_t num_uncommits_left = _to_uncommit / limit;
      const double time_left = double(ZUncommitDelay) - time_since_last_uncommit;
      if (time_left < *timeout * num_uncommits_left) {
        // Running out of time, speed up.
        uint64_t new_timeout = uint64_t(std::floor(time_left / double(num_uncommits_left + 1)));
        *timeout = new_timeout;
      }
    } else {
      // We are about to start uncommitting
      _to_uncommit = _cache.reset_min();
      _last_uncommit = now;

      const size_t split = _to_uncommit / limit + 1;
      uint64_t new_timeout = ZUncommitDelay / split;
      *timeout = new_timeout;
    }

    // Never uncommit below min capacity.
    const size_t retain = MAX2(_used, _min_capacity);
    const size_t release = _capacity - retain;
    const size_t flush = MIN3(release, limit, _to_uncommit);

    if (flush == 0) {
      // Nothing to flush
      return 0;
    }

    // Flush memory from the mapped cache to uncommit
    flushed = _cache.remove_from_min(flush, &flushed_vmems);
    if (flushed == 0) {
      // Nothing flushed
      return 0;
    }

    // Record flushed memory as claimed and how much we've flushed for this NUMA node
    Atomic::add(&_claimed, flushed);
    _to_uncommit -= flushed;
  }

  // Unmap and uncommit flushed memory
  ZArrayIterator<ZVirtualMemory> it(&flushed_vmems);
  for (ZVirtualMemory vmem; it.next(&vmem);) {
    unmap_virtual(vmem);
    uncommit_physical(vmem);
    free_physical(vmem);
    free_virtual(vmem);
  }

  {
    SuspendibleThreadSetJoiner sts_joiner;
    ZLocker<ZLock> locker(&_page_allocator->_lock);

    // Adjust claimed and capacity to reflect the uncommit
    Atomic::sub(&_claimed, flushed);
    decrease_capacity(flushed, false /* set_max_capacity */);
  }

  return flushed;
}

const ZUncommitter& ZAllocNode::uncommitter() const {
  return _uncommitter;
}

ZUncommitter& ZAllocNode::uncommitter() {
  return _uncommitter;
}

void ZAllocNode::threads_do(ThreadClosure* tc) const {
  tc->do_thread(const_cast<ZUncommitter*>(&_uncommitter));
}

void ZAllocNode::print_on(outputStream* st) const {
  st->print("  Node %u", _numa_id);
  st->fill_to(17 + st->indentation());
  st->print_cr("used %zuM, capacity %zuM, max capacity %zuM",
               _used / M, _capacity / M, _max_capacity / M);

  _cache.print_on(st);
}

void ZAllocNode::print_extended_on_error(outputStream* st) const {
  st->print_cr(" Node %u", _numa_id);
  _cache.print_extended_on(st);
}

void ZAllocNode::claim_physical(const ZVirtualMemory& vmem) {
  // We do not verify the virtual memory association as multi-node allocation
  // allocates new physical segments directly in the final virtual memory range,
  // which may not be associated with the current node.

  ZPhysicalMemoryManager& manager = physical_memory_manager();
  zbacking_index* const pmem = physical_mappings_addr(vmem);
  const size_t size = vmem.size();

  // Alloc physical memory
  manager.alloc(pmem, size, _numa_id);
}

void ZAllocNode::free_physical(const ZVirtualMemory& vmem) {
  // We do not verify the virtual memory association as multi-node allocation
  // allocates and commits new physical segments directly in the final virtual
  // memory range, which may not be associated with the current node. If a
  // commit fails it will also be used to free.

  ZPhysicalMemoryManager& manager = physical_memory_manager();
  zbacking_index* const pmem = physical_mappings_addr(vmem);
  const size_t size = vmem.size();

  // Free physical memory
  manager.free(pmem, size, _numa_id);
}

size_t ZAllocNode::commit_physical(const ZVirtualMemory& vmem) {
  // We do not verify the virtual memory association as multi-node allocation
  // commits new physical segments directly in the final virtual memory range,
  // which may not be associated with the current node.

  ZPhysicalMemoryManager& manager = physical_memory_manager();
  zbacking_index* const pmem = physical_mappings_addr(vmem);
  const size_t size = vmem.size();

  // Commit physical memory
  return manager.commit(pmem, size, _numa_id);
}

size_t ZAllocNode::uncommit_physical(const ZVirtualMemory& vmem) {
  assert(ZUncommit, "should not uncommit when uncommit is disabled");
  verify_virtual_memory_association(vmem);

  ZPhysicalMemoryManager& manager = physical_memory_manager();
  zbacking_index* const pmem = physical_mappings_addr(vmem);
  const size_t size = vmem.size();

  // Uncommit physical memory
  return manager.uncommit(pmem, size);
}

void ZAllocNode::map_virtual_to_physical(const ZVirtualMemory& vmem) {
  verify_virtual_memory_association(vmem);

  ZPhysicalMemoryManager& manager = physical_memory_manager();
  const zoffset offset = vmem.start();
  zbacking_index* const pmem = physical_mappings_addr(vmem);
  const size_t size = vmem.size();

  // Map virtual memory to physical memory
  manager.map(offset, pmem, size, _numa_id);
}

ZVirtualMemory ZAllocNode::claim_virtual(size_t size, bool force_low_address) {
  ZVirtualMemoryManager& manager = virtual_memory_manager();

  return manager.remove(size, _numa_id, force_low_address);
}

size_t ZAllocNode::claim_virtual(size_t size, ZArray<ZVirtualMemory>* vmems_out) {
  ZVirtualMemoryManager& manager = virtual_memory_manager();

  return manager.remove_low_address_many_at_most(size, _numa_id, vmems_out);
}

void ZAllocNode::unmap_virtual(const ZVirtualMemory& vmem) {
  verify_virtual_memory_association(vmem);

  ZPhysicalMemoryManager& manager = physical_memory_manager();
  const zoffset offset = vmem.start();
  zbacking_index* const pmem = physical_mappings_addr(vmem);
  const size_t size = vmem.size();

  // Unmap virtual memory from physical memory
  manager.unmap(offset, pmem, size);
}

void ZAllocNode::free_virtual(const ZVirtualMemory& vmem) {
  verify_virtual_memory_association(vmem);

  ZVirtualMemoryManager& manager = virtual_memory_manager();

  // Free virtual memory
  manager.insert(vmem, _numa_id);
}

void ZAllocNode::shuffle_virtual(const ZVirtualMemory& vmem, ZArray<ZVirtualMemory>* vmems_out) {
  verify_virtual_memory_association(vmem);

  ZVirtualMemoryManager& manager = virtual_memory_manager();

  // Shuffle virtual memory
  manager.shuffle_to_low_addresses(vmem, _numa_id, vmems_out);
}

ZVirtualMemory ZAllocNode::shuffle_virtual(size_t size, ZArray<ZVirtualMemory>* vmems_in_out) {
  verify_virtual_memory_association(vmems_in_out);

  ZVirtualMemoryManager& manager = virtual_memory_manager();

  // Shuffle virtual memory
  return manager.shuffle_to_low_addresses_and_remove_contiguous(size, _numa_id, vmems_in_out);
}

static void pretouch_memory(zoffset start, size_t size) {
  // At this point we know that we have a valid zoffset / zaddress.
  const zaddress zaddr = ZOffset::address(start);
  const uintptr_t addr = untype(zaddr);
  const size_t page_size = ZLargePages::is_explicit() ? ZGranuleSize : os::vm_page_size();
  os::pretouch_memory((void*)addr, (void*)(addr + size), page_size);
}

class ZPreTouchTask : public ZTask {
private:
  volatile uintptr_t _current;
  const uintptr_t    _end;

public:
  ZPreTouchTask(zoffset start, zoffset_end end)
    : ZTask("ZPreTouchTask"),
      _current(untype(start)),
      _end(untype(end)) {}

  virtual void work() {
    const size_t size = ZGranuleSize;

    for (;;) {
      // Claim an offset for this thread
      const uintptr_t claimed = Atomic::fetch_then_add(&_current, size);
      if (claimed >= _end) {
        // Done
        break;
      }

      // At this point we know that we have a valid zoffset / zaddress.
      const zoffset offset = to_zoffset(claimed);

      // Pre-touch the granule
      pretouch_memory(offset, size);
    }
  }
};

bool ZAllocNode::prime(ZWorkers* workers, size_t size) {
  if (size == 0) {
    return true;
  }

  // Claim virtual memory
  const ZVirtualMemory vmem = claim_virtual(size, true /* force_low_address */);

  // Increase capacity
  increase_capacity(size);

  // Claim the backing physical memory
  claim_physical(vmem);

  // Commit the claimed physical memory
  const size_t committed = commit_physical(vmem);

  if (committed != vmem.size()) {
    // This is a failure state. We do not cleanup the maybe partially committed memory.
    return false;
  }

  map_virtual_to_physical(vmem);

  check_numa_mismatch(vmem, _numa_id);

  if (AlwaysPreTouch) {
    // Pre-touch memory
    ZPreTouchTask task(vmem.start(), vmem.end());
    workers->run_all(&task);
  }

  // We don't have to take a lock here as no other threads will access the cache
  // until we're finished
  _cache.insert(vmem);

  return true;
}

ZVirtualMemory ZAllocNode::remap_harvested_and_claim_virtual(ZMemoryAllocation* allocation) {
  verify_memory_allocation_association(allocation);

  // Unmap virtual memory
  ZArrayIterator<ZVirtualMemory> iter(allocation->partial_vmems());
  for (ZVirtualMemory vmem; iter.next(&vmem);) {
    unmap_virtual(vmem);
  }

  const int num_granules = (int)(allocation->harvested() >> ZGranuleSizeShift);
  ZSegmentStash segments(&physical_mappings(), num_granules);

  // Stash segments
  segments.stash(allocation->partial_vmems());

  // Shuffle virtual memory. We attempt to allocate enough memory to cover the entire
  // allocation size, not just for the harvested memory.
  const ZVirtualMemory result = shuffle_virtual(allocation->size(), allocation->partial_vmems());

  // Restore segments
  if (!result.is_null()) {
    segments.pop_all(result);
  } else {
    segments.pop_all(allocation->partial_vmems());
  }

  if (result.is_null()) {
    // Before returning harvested memory to the cache it must be mapped.
    ZArrayIterator<ZVirtualMemory> iter(allocation->partial_vmems());
    for (ZVirtualMemory vmem; iter.next(&vmem);) {
      map_virtual_to_physical(vmem);
    }
  }

  return result;
}

ZVirtualMemory ZAllocNode::claim_virtual_memory(ZMemoryAllocation* allocation) {
  verify_memory_allocation_association(allocation);

  if (allocation->harvested() > 0) {
    // If we have harvested anything, we claim virtual memory from the harvested
    // vmems, and perhaps also allocate more to match the allocation request.
    return remap_harvested_and_claim_virtual(allocation);
  } else {
    // Just try to claim virtual memory
    return claim_virtual(allocation->size(), true /* force_low_address */);
  }
}

ZVirtualMemory ZAllocNode::commit_increased_capacity(ZMemoryAllocation* allocation, const ZVirtualMemory& vmem) {
  const size_t already_committed = allocation->harvested();

  const ZVirtualMemory already_committed_vmem = vmem.first_part(already_committed);
  const ZVirtualMemory to_be_committed_vmem = vmem.last_part(already_committed);

  // Try to commit all physical memory, commit_physical frees both the virtual
  // and physical parts that correspond to the memory that failed to be committed.
  const size_t committed = commit_physical(to_be_committed_vmem);

  // We got more committed memory
  const ZVirtualMemory total_committed_vmem(already_committed_vmem.start(), already_committed_vmem.size() + committed);

  // Keep track of the committed amount
  allocation->set_committed_capacity(committed);

  return total_committed_vmem;
}

void ZAllocNode::map_memory(ZMemoryAllocation* allocation, const ZVirtualMemory& vmem) {
  _page_allocator->sort_segments_physical(vmem);
  map_virtual_to_physical(vmem);

  check_numa_mismatch(vmem, allocation->numa_id());
}

bool ZAllocNode::commit_and_map_memory(ZMemoryAllocation* allocation, const ZVirtualMemory& vmem) {
  const size_t already_committed = allocation->harvested();

  // Commit more memory if necessary
  const ZVirtualMemory committed_vmem = (already_committed == vmem.size())
      ? vmem // Already fully committed
      : commit_increased_capacity(allocation, vmem);

  // Check if we managed to commit all we requested
  if (committed_vmem.size() != vmem.size()) {
    // Failed to commit all that we requested

    // Free the uncommitted memory
    const ZVirtualMemory not_commited_vmem = vmem.last_part(committed_vmem.size());
    free_physical(not_commited_vmem);
    free_virtual(not_commited_vmem);

    allocation->set_commit_failed();
  }

  if (committed_vmem.size() == 0)  {
    // We have not managed to get any committed memory at all.
    return false;
  }

  // Map all the committed memory
  map_memory(allocation, committed_vmem);

  if (committed_vmem.size() != vmem.size()) {
    // Register the committed and mapped memory
    allocation->partial_vmems()->append(committed_vmem);

    return false;
  }

  return true;
}

void ZAllocNode::free_memory_alloc_failed(ZMemoryAllocation* allocation) {
  verify_memory_allocation_association(allocation);

  // Only decrease the overall used and not the generation used,
  // since the allocation failed and generation used wasn't bumped.
  decrease_used(allocation->size());

  size_t freed = 0;

  // Free mapped memory
  ZArrayIterator<ZVirtualMemory> iter(allocation->partial_vmems());
  for (ZVirtualMemory vmem; iter.next(&vmem);) {
    freed += vmem.size();
    _cache.insert(vmem);
  }
  assert(allocation->harvested() + allocation->committed_capacity() == freed, "must have freed all");

  // Adjust capacity to reflect the failed capacity increase
  const size_t remaining = allocation->size() - freed;
  if (remaining > 0) {
    const bool set_max_capacity = allocation->commit_failed();
    decrease_capacity(remaining, set_max_capacity);
    if (set_max_capacity) {
      log_error_p(gc)("Forced to lower max Java heap size from "
                      "%zuM(%.0f%%) to %zuM(%.0f%%) (NUMA id %d)",
                      _current_max_capacity / M, percent_of(_current_max_capacity, _max_capacity),
                      _capacity / M, percent_of(_capacity, _max_capacity),
                      allocation->numa_id());
    }
  }
}

class ZMultiNodeTracker : CHeapObj<mtGC> {
private:
  struct Element {
    ZVirtualMemory _vmem;
    uint32_t       _numa_id;
  };

  ZArray<Element> _map;

  ZMultiNodeTracker(int capacity)
    : _map(capacity) {}

  const ZArray<Element>* map() const {
    return &_map;
  }

  ZArray<Element>* map() {
    return &_map;
  }

public:
  static ZMultiNodeTracker* create(const ZMultiNodeAllocation* multi_node_allocation, const ZVirtualMemory vmem) {
    const ZArray<ZMemoryAllocation*>* const partial_allocations = multi_node_allocation->allocations();

    ZMultiNodeTracker* const tracker = new ZMultiNodeTracker(partial_allocations->length());

    ZVirtualMemory remaining = vmem;

    // Each partial allocation is mapped to the virtual memory in order
    for (ZMemoryAllocation* partial_allocation : *partial_allocations) {
      // Track each separate vmem's numa node
      const ZVirtualMemory partial_vmem = remaining.split_from_front(partial_allocation->size());
      const uint32_t numa_id = partial_allocation->numa_id();
      tracker->map()->push({partial_vmem, numa_id});
    }

    return tracker;
  }

  static void free_and_destroy(ZPageAllocator* allocator, ZPage* page) {
    const uint32_t numa_nodes = ZNUMA::count();

    // Extract data and destroy page
    const ZVirtualMemory vmem = page->virtual_memory();
    const ZGenerationId id = page->generation_id();
    const ZMultiNodeTracker* const tracker = page->multi_node_tracker();
    allocator->safe_destroy_page(page);

    // Keep track of to be inserted vmems
    struct PerNUMAData : public CHeapObj<mtGC> {
      ZArray<ZVirtualMemory> _vmems{};
      size_t _mapped = 0;
      size_t _uncommitted = 0;
    };
    PerNUMAData* const per_numa_vmems = new PerNUMAData[numa_nodes];
    ZAllocNode& vmem_node = allocator->node_from_vmem(vmem);

    // Remap memory back to original numa node
    ZArrayIterator<Element> iter(tracker->map());
    for (Element partial_allocation; iter.next(&partial_allocation);) {
      ZVirtualMemory remaining_vmem = partial_allocation._vmem;
      const uint32_t numa_id = partial_allocation._numa_id;

      PerNUMAData& numa_data = per_numa_vmems[numa_id];
      ZArray<ZVirtualMemory>* const numa_memory_vmems = &numa_data._vmems;
      const size_t size = remaining_vmem.size();
      ZAllocNode& original_node = allocator->node_from_numa_id(numa_id);

      // Allocate new virtual address ranges
      const int start_index = numa_memory_vmems->length();
      const size_t claimed_virtual = original_node.claim_virtual(remaining_vmem.size(), numa_memory_vmems);

      // Remap to the newly allocated virtual address ranges
      size_t mapped = 0;
      ZArrayIterator<ZVirtualMemory> iter(numa_memory_vmems, start_index);
      for (ZVirtualMemory to_vmem; iter.next(&to_vmem);) {
        ZVirtualMemory from_vmem = remaining_vmem.split_from_front(to_vmem.size());

        // Copy physical segments
        allocator->copy_physical_segments(to_vmem.start(), from_vmem);

        // Unmap from_vmem
        vmem_node.unmap_virtual(from_vmem);

        // Map to_vmem
        original_node.map_virtual_to_physical(to_vmem);

        mapped += to_vmem.size();
      }

      assert(claimed_virtual == mapped, "must have mapped all claimed virtual memory");
      assert(size == mapped + remaining_vmem.size(), "must cover whole range");

      if (remaining_vmem.size() != 0) {
        // Failed to get vmem for all memory, unmap, uncommit and free the remaining
        original_node.unmap_virtual(remaining_vmem);
        original_node.uncommit_physical(remaining_vmem);
        original_node.free_physical(remaining_vmem);
      }

      // Keep track of the per numa data
      numa_data._mapped += mapped;
      numa_data._uncommitted += remaining_vmem.size();
    }

    // Free the virtual memory
    vmem_node.free_virtual(vmem);

    {
      ZLocker<ZLock> locker(&allocator->_lock);

      ZPerNUMAIterator<ZAllocNode> iter = allocator->alloc_node_iterator();
      for (ZAllocNode* node; iter.next(&node);) {
        const uint32_t numa_id = node->numa_id();
        PerNUMAData& numa_data = per_numa_vmems[numa_id];

        // Update accounting
        node->decrease_used(numa_data._mapped + numa_data._uncommitted);
        node->decrease_used_generation(id, numa_data._mapped + numa_data._uncommitted);
        node->decrease_capacity(numa_data._uncommitted, false /* set_max_capacity */);

        // Reinsert vmems
        ZArrayIterator<ZVirtualMemory> iter(&numa_data._vmems);
        for (ZVirtualMemory vmem; iter.next(&vmem);) {
          node->cache()->insert(vmem);
        }
      }

      // Try satisfy stalled allocations
      allocator->satisfy_stalled();
    }

    // Free up the allocated memory
    delete[] per_numa_vmems;
    delete tracker;
  }

  static void promote(ZPageAllocator* allocator, const ZPage* from, const ZPage* to) {
    ZMultiNodeTracker* const tracker = from->multi_node_tracker();
    assert(tracker == to->multi_node_tracker(), "should have the same tracker");

    ZArrayIterator<Element> iter(tracker->map());
    for (Element partial_allocation; iter.next(&partial_allocation);) {
      const size_t size = partial_allocation._vmem.size();
      const uint32_t numa_id = partial_allocation._numa_id;
      ZAllocNode& node = allocator->node_from_numa_id(numa_id);
      node.promote_used(size);
    }
  }
};

ZPageAllocator::ZPageAllocator(size_t min_capacity,
                               size_t initial_capacity,
                               size_t soft_max_capacity,
                               size_t max_capacity)
  : _lock(),
    _virtual(max_capacity),
    _physical(max_capacity),
    _physical_mappings(ZAddressOffsetMax),
    _min_capacity(min_capacity),
    _initial_capacity(initial_capacity),
    _max_capacity(max_capacity),
    _alloc_nodes(ZValueIdTagType{}, this),
    _stalled(),
    _safe_destroy(),
    _initialized(false) {

  if (!_virtual.is_initialized() || !_physical.is_initialized()) {
    return;
  }

  log_info_p(gc, init)("Min Capacity: %zuM", min_capacity / M);
  log_info_p(gc, init)("Initial Capacity: %zuM", initial_capacity / M);
  log_info_p(gc, init)("Max Capacity: %zuM", max_capacity / M);
  log_info_p(gc, init)("Soft Max Capacity: %zuM", soft_max_capacity / M);
  if (ZPageSizeMedium > 0) {
    log_info_p(gc, init)("Medium Page Size: %zuM", ZPageSizeMedium / M);
  } else {
    log_info_p(gc, init)("Medium Page Size: N/A");
  }
  log_info_p(gc, init)("Pre-touch: %s", AlwaysPreTouch ? "Enabled" : "Disabled");

  // Warn if system limits could stop us from reaching max capacity
  _physical.warn_commit_limits(max_capacity);

  // Check if uncommit should and can be enabled
  _physical.try_enable_uncommit(min_capacity, max_capacity);

  // Successfully initialized
  _initialized = true;
}

bool ZPageAllocator::is_initialized() const {
  return _initialized;
}

bool ZPageAllocator::prime_cache(ZWorkers* workers, size_t size) {
  ZPerNUMAIterator<ZAllocNode> iter = alloc_node_iterator();
  for (ZAllocNode* node; iter.next(&node);) {
    const uint32_t numa_id = node->numa_id();
    const size_t to_prime = ZNUMA::calculate_share(numa_id, size);

    if (!node->prime(workers, to_prime)) {
      return false;
    }
  }

  return true;
}

size_t ZPageAllocator::initial_capacity() const {
  return _initial_capacity;
}

size_t ZPageAllocator::min_capacity() const {
  return _min_capacity;
}

size_t ZPageAllocator::max_capacity() const {
  return _max_capacity;
}

size_t ZPageAllocator::soft_max_capacity() const {
  const size_t current_max_capacity = ZPageAllocator::current_max_capacity();
  const size_t soft_max_heapsize = Atomic::load(&SoftMaxHeapSize);
  return MIN2(soft_max_heapsize, current_max_capacity);
}

size_t ZPageAllocator::current_max_capacity() const {
  size_t current_max_capacity = 0;
  ZPerNUMAConstIterator<ZAllocNode> iter = alloc_node_iterator();
  for (const ZAllocNode* node; iter.next(&node);) {
    current_max_capacity += Atomic::load(&node->_current_max_capacity);
  }

  return current_max_capacity;
}

size_t ZPageAllocator::capacity() const {
  size_t capacity = 0;
  ZPerNUMAConstIterator<ZAllocNode> iter = alloc_node_iterator();
  for (const ZAllocNode* node; iter.next(&node);) {
    capacity += Atomic::load(&node->_capacity);
  }
  return capacity;
}

size_t ZPageAllocator::used() const {
  size_t used = 0;
  ZPerNUMAConstIterator<ZAllocNode> iter = alloc_node_iterator();
  for (const ZAllocNode* node; iter.next(&node);) {
    used += Atomic::load(&node->_used);
  }
  return used;
}

size_t ZPageAllocator::used_generation(ZGenerationId id) const {
  size_t used_generation = 0;
  ZPerNUMAConstIterator<ZAllocNode> iter = alloc_node_iterator();
  for (const ZAllocNode* node; iter.next(&node);) {
    used_generation += Atomic::load(&node->_used_generations[(int)id]);
  }
  return used_generation;
}

size_t ZPageAllocator::unused() const {
  ssize_t capacity = 0;
  ssize_t used = 0;
  ssize_t claimed = 0;

  ZPerNUMAConstIterator<ZAllocNode> iter = alloc_node_iterator();
  for (const ZAllocNode* node; iter.next(&node);) {
    capacity += (ssize_t)Atomic::load(&node->_capacity);
    used += (ssize_t)Atomic::load(&node->_used);
    claimed += (ssize_t)Atomic::load(&node->_claimed);
  }

  const ssize_t unused = capacity - used - claimed;
  return unused > 0 ? (size_t)unused : 0;
}

ZPageAllocatorStats ZPageAllocator::stats(ZGeneration* generation) const {
  ZLocker<ZLock> locker(&_lock);

  ZPageAllocatorStats stats(_min_capacity,
                            _max_capacity,
                            soft_max_capacity(),
                            generation->freed(),
                            generation->promoted(),
                            generation->compacted(),
                            _stalled.size());

  // Aggregate per ZAllocNode stats
  const int gen_id = (int)generation->id();
  ZPerNUMAConstIterator<ZAllocNode> iter(&_alloc_nodes);
  for (const ZAllocNode* node; iter.next(&node);) {
    stats.increment_stats(node->_capacity,
                          node->_used,
                          node->_collection_stats[gen_id]._used_high,
                          node->_collection_stats[gen_id]._used_low,
                          node->_used_generations[gen_id]);
  }

  return stats;
}

void ZPageAllocator::reset_statistics(ZGenerationId id) {
  assert(SafepointSynchronize::is_at_safepoint(), "Should be at safepoint");

  ZPerNUMAIterator<ZAllocNode> iter(&_alloc_nodes);
  for (ZAllocNode* node; iter.next(&node);) {
    node->reset_statistics(id);
  }
}

const ZAllocNode& ZPageAllocator::node_from_numa_id(uint32_t numa_id) const {
  return _alloc_nodes.get(numa_id);
}

ZAllocNode& ZPageAllocator::node_from_numa_id(uint32_t numa_id) {
  return _alloc_nodes.get(numa_id);
}

ZAllocNode& ZPageAllocator::node_from_vmem(const ZVirtualMemory& vmem) {
  return node_from_numa_id(_virtual.get_numa_id(vmem));
}

void ZPageAllocator::promote_used(const ZPage* from, const ZPage* to) {
  assert(from->size() == to->size(), "pages are the same size");
  assert(from->start() == to->start(), "pages start at same offset");

  if (from->is_multi_node()) {
    ZMultiNodeTracker::promote(this, from, to);
  } else {
    const size_t size = from->size();
    ZAllocNode &node = node_from_vmem(from->virtual_memory());
    node.promote_used(size);
  }
}

size_t ZPageAllocator::count_segments_physical(const ZVirtualMemory& vmem) {
  return _physical.count_segments(_physical_mappings.addr(vmem.start()), vmem.size());
}

void ZPageAllocator::sort_segments_physical(const ZVirtualMemory& vmem) {
  sort_zbacking_index_array(_physical_mappings.addr(vmem.start()), vmem.size_in_granules());
}

void ZPageAllocator::remap_and_defragment(const ZVirtualMemory& vmem, ZArray<ZVirtualMemory>* entries) {
  ZAllocNode& node = node_from_vmem(vmem);

  // If no lower address can be found, don't remap/defrag
  if (_virtual.lowest_available_address(_virtual.get_numa_id(vmem)) > vmem.start()) {
    entries->append(vmem);
    return;
  }

  ZStatInc(ZCounterDefragment);

  // Synchronously unmap the virtual memory
  node.unmap_virtual(vmem);

  // Stash segments
  ZSegmentStash segments(&_physical_mappings, (int)vmem.size_in_granules());
  segments.stash(vmem);

  // Shuffle vmem - put new vmems in entries
  const int start_index = entries->length();
  node.shuffle_virtual(vmem, entries);
  const int num_vmems = entries->length() - start_index;

  // Restore segments
  segments.pop(entries, num_vmems);

  // The entries array may contain entries from other defragmentations as well,
  // so we only operate on the last ranges that we have just inserted
  ZArrayIterator<ZVirtualMemory> iter(entries, start_index);
  for (ZVirtualMemory v; iter.next(&v);) {
    node.map_virtual_to_physical(v);
    pretouch_memory(v.start(), v.size());
  }
}

static void check_out_of_memory_during_initialization() {
  if (!is_init_completed()) {
    vm_exit_during_initialization("java.lang.OutOfMemoryError", "Java heap too small");
  }
}

bool ZPageAllocator::alloc_page_stall(ZPageAllocation* allocation) {
  ZStatTimer timer(ZCriticalPhaseAllocationStall);
  EventZAllocationStall event;

  // We can only block if the VM is fully initialized
  check_out_of_memory_during_initialization();

  // Start asynchronous minor GC
  const ZDriverRequest request(GCCause::_z_allocation_stall, ZYoungGCThreads, 0);
  ZDriver::minor()->collect(request);

  // Wait for allocation to complete or fail
  const bool result = allocation->wait();

  {
    // Guard deletion of underlying semaphore. This is a workaround for
    // a bug in sem_post() in glibc < 2.21, where it's not safe to destroy
    // the semaphore immediately after returning from sem_wait(). The
    // reason is that sem_post() can touch the semaphore after a waiting
    // thread have returned from sem_wait(). To avoid this race we are
    // forcing the waiting thread to acquire/release the lock held by the
    // posting thread. https://sourceware.org/bugzilla/show_bug.cgi?id=12674
    ZLocker<ZLock> locker(&_lock);
  }

  // Send event
  event.commit((u8)allocation->type(), allocation->size());

  return result;
}

bool ZPageAllocator::claim_capacity_multi_node(ZMultiNodeAllocation* multi_node_allocation, uint32_t start_node) {
  const size_t size = multi_node_allocation->size();
  const uint32_t numa_nodes = ZNUMA::count();
  const size_t split_size = align_up(size / numa_nodes, ZGranuleSize);

  size_t remaining = size;

  const auto do_claim_one_node = [&](ZAllocNode& node, bool claim_evenly) {
    if (remaining == 0) {
      // All memory claimed
      return false;
    }

    const size_t max_alloc_size = claim_evenly ? MIN2(split_size, remaining) : remaining;

    // This guarantees that claim_physical below will succeed
    const size_t alloc_size = MIN2(max_alloc_size, node.available());

    // Skip over empty allocations
    if (alloc_size == 0) {
      // Continue
      return true;
    }

    ZMemoryAllocation partial_allocation(alloc_size);

    // Claim capacity for this allocation - this should succeed
    const bool result = node.claim_capacity(&partial_allocation);
    assert(result, "Should have succeeded");

    // Register allocation
    multi_node_allocation->register_allocation(partial_allocation);

    // Update remaining
    remaining -= alloc_size;

    // Continue
    return true;
  };

  // Loops over every node and allocates memory from nodes
  const auto do_claim_each_node = [&](bool claim_evenly) {
    for (uint32_t i = 0; i < numa_nodes; ++i) {
      const uint32_t numa_id = (start_node + i) % numa_nodes;
      ZAllocNode& node = _alloc_nodes.get(numa_id);

      if (!do_claim_one_node(node, claim_evenly)) {
        // All memory claimed
        break;
      }
    }
  };

  // Try to claim from multiple nodes

  // Try to claim up to split_size on each node
  do_claim_each_node(true  /* claim_evenly */);

  // Try claim the remaining
  do_claim_each_node(false /* claim_evenly */);

  return remaining == 0;
}

bool ZPageAllocator::claim_capacity_single_node(ZSingleNodeAllocation* single_node_allocation, uint32_t numa_id) {
  ZAllocNode& node = _alloc_nodes.get(numa_id);

  return node.claim_capacity(single_node_allocation->allocation());
}

size_t ZPageAllocator::sum_available() const {
  const uint32_t numa_nodes = ZNUMA::count();

  size_t total = 0;

  for (uint32_t i = 0; i < numa_nodes; ++i) {
    total += _alloc_nodes.get(i).available();
  }

  return total;
}

bool ZPageAllocator::claim_capacity(ZPageAllocation* allocation) {
  const uint32_t start_node = allocation->initiating_numa_id();
  const uint32_t numa_nodes = ZNUMA::count();

  // Round robin single-node claiming

  for (uint32_t i = 0; i < numa_nodes; ++i) {
    const uint32_t numa_id = (start_node + i) % numa_nodes;

    if (claim_capacity_single_node(allocation->single_node_allocation(), numa_id)) {
      return true;
    }
  }

  if (numa_nodes <= 1 || sum_available() < allocation->size()) {
    // Multi-node claiming is not possible
    return false;
  }

  // Multi-node claiming

  // Flip allocation to multi-node allocation
  allocation->initiate_multi_node_allocation();

  ZMultiNodeAllocation* const multi_node_allocation = allocation->multi_node_allocation();

  if (claim_capacity_multi_node(multi_node_allocation, start_node)) {
    return true;
  }

  // May have partially succeeded, undo any partial allocations
  free_memory_alloc_failed_multi_node(multi_node_allocation);

  return false;
}

bool ZPageAllocator::claim_capacity_or_stall(ZPageAllocation* allocation) {
  {
    ZLocker<ZLock> locker(&_lock);

    // Try to claim memory
    if (claim_capacity(allocation)) {
      return true;
    }

    // Failed to claim memory
    if (allocation->flags().non_blocking()) {
      // Don't stall
      return false;
    }

    // Enqueue allocation request
    _stalled.insert_last(allocation);
  }

  // Stall
  return alloc_page_stall(allocation);
}

ZVirtualMemory ZPageAllocator::satisfied_from_cache_vmem(const ZPageAllocation* allocation) const {
  if (allocation->is_multi_node()) {
    // Multi-node allocations are always harvested and/or committed, so there's never a
    // satisfying vmem from the caches.
    return {};
  }

  return allocation->satisfied_from_cache_vmem();
}

void ZPageAllocator::copy_physical_segments(zoffset to, const ZVirtualMemory& from) {
  zbacking_index* const dest = _physical_mappings.addr(to);
  const zbacking_index* const src = _physical_mappings.addr(from.start());
  const size_t num_granules = from.size_in_granules();

  ZUtils::copy_disjoint(dest, src, num_granules);
}

void ZPageAllocator::copy_claimed_physical_multi_node(ZMultiNodeAllocation* multi_node_allocation, const ZVirtualMemory& vmem) {
  // Start at the new dest offset
  zoffset_end offset = to_zoffset_end(vmem.start());

  for (const ZMemoryAllocation* partial_allocation : *multi_node_allocation->allocations()) {
    // Iterate over all partial vmems and copy physical segments into the
    // partial_allocations' destination offset
    for (const ZVirtualMemory partial_vmem : *partial_allocation->partial_vmems()) {
      // Copy physical segments
      copy_physical_segments(to_zoffset(offset), partial_vmem);

      // Advance to next partial_vmem's offset
      offset += partial_vmem.size();
    }

    offset += partial_allocation->increased_capacity();

    assert(partial_allocation->size() == partial_allocation->harvested() + partial_allocation->increased_capacity(),
           "Must be %zu == %zu + %zu",
           partial_allocation->size(), partial_allocation->harvested(), partial_allocation->increased_capacity());
  }

  assert(offset == vmem.end(),
         "All memory should have been accounted for "
         "offset: " PTR_FORMAT " vmem.end(): " PTR_FORMAT,
         untype(offset), untype(vmem.end()));
}

ZVirtualMemory ZPageAllocator::claim_virtual_memory_multi_node(ZMultiNodeAllocation* multi_node_allocation) {
  const uint32_t numa_nodes = ZNUMA::count();
  const size_t size = multi_node_allocation->size();

  ZPerNUMAIterator<ZAllocNode> iter = alloc_node_iterator();
  for (ZAllocNode* node; iter.next(&node);) {
    ZVirtualMemory vmem = node->claim_virtual(size, false /* force_low_address */);
    if (!vmem.is_null()) {
      // Copy claimed multi-node vmems, we leave the old vmems mapped until after
      // we have committed. In case committing fails we can simply reinsert the
      // inital vmems.
      copy_claimed_physical_multi_node(multi_node_allocation, vmem);

      return vmem;
    }
  }

  return {};
}

ZVirtualMemory ZPageAllocator::claim_virtual_memory_single_node(ZSingleNodeAllocation* single_node_allocation) {
  ZMemoryAllocation* const allocation = single_node_allocation->allocation();
  const uint32_t numa_id = allocation->numa_id();
  ZAllocNode& node = node_from_numa_id(numa_id);

  return node.claim_virtual_memory(allocation);
}

ZVirtualMemory ZPageAllocator::claim_virtual_memory(ZPageAllocation* allocation) {
  // Note: that the single-node performs "shuffling" of already harvested
  // vmem(s), while the multi-node searches for available virtual memory
  // area without shuffling.

  if (allocation->is_multi_node()) {
    return claim_virtual_memory_multi_node(allocation->multi_node_allocation());
  } else {
    return claim_virtual_memory_single_node(allocation->single_node_allocation());
  }
}

void ZPageAllocator::claim_physical_for_increased_capacity(ZMemoryAllocation* allocation, const ZVirtualMemory& vmem) {
  // The previously harvested memory is memory that has already been committed
  // and mapped. The rest of the vmem gets physical memory assigned here and
  // will be committed in a subsequent function.

  const size_t already_committed = allocation->harvested();
  const size_t non_committed = allocation->size() - already_committed;
  const size_t increased_capacity = allocation->increased_capacity();

  assert(non_committed == increased_capacity,
         "Mismatch non_committed: " PTR_FORMAT " increased_capacity: " PTR_FORMAT,
         non_committed, increased_capacity);

  if (non_committed > 0) {
    ZAllocNode& node = node_from_numa_id(allocation->numa_id());
    ZVirtualMemory non_committed_vmem = vmem.last_part(already_committed);
    node.claim_physical(non_committed_vmem);
  }
}

void ZPageAllocator::claim_physical_for_increased_capacity_multi_node(const ZMultiNodeAllocation* multi_node_allocation, const ZVirtualMemory& vmem) {
  ZVirtualMemory remaining = vmem;

  for (ZMemoryAllocation* allocation : *multi_node_allocation->allocations()) {
    const ZVirtualMemory partial = remaining.split_from_front(allocation->size());
    claim_physical_for_increased_capacity(allocation, partial);
  }
}

void ZPageAllocator::claim_physical_for_increased_capacity_single_node(ZSingleNodeAllocation* single_node_allocation, const ZVirtualMemory& vmem) {
  claim_physical_for_increased_capacity(single_node_allocation->allocation(), vmem);
}

void ZPageAllocator::claim_physical_for_increased_capacity(ZPageAllocation* allocation, const ZVirtualMemory& vmem) {
  assert(allocation->size() == vmem.size(), "vmem should be the final entry");

  if (allocation->is_multi_node()) {
    claim_physical_for_increased_capacity_multi_node(allocation->multi_node_allocation(), vmem);
  } else {
    claim_physical_for_increased_capacity_single_node(allocation->single_node_allocation(), vmem);
  }
}

bool ZPageAllocator::commit_memory_multi_node(ZMultiNodeAllocation* multi_node_allocation, const ZVirtualMemory& vmem) {
  size_t offset = 0;
  ZArrayIterator<ZMemoryAllocation*> iter(multi_node_allocation->allocations());
  for (ZMemoryAllocation* allocation; iter.next(&allocation); offset += allocation->size()) {
    if (allocation->harvested() == allocation->size()) {
      // The allocation has already been fully satisfied by harvesting.
      continue;
    }

    // Partition off this partial allocation's memory range
    const ZVirtualMemory partial_vmem = vmem.partition(offset, allocation->size());

    // Extract the part that we need to commit
    const ZVirtualMemory to_commit_vmem = partial_vmem.last_part(allocation->harvested());

    // Try to commit
    ZAllocNode& node = node_from_numa_id(allocation->numa_id());
    const size_t to_commit = to_commit_vmem.size();
    const size_t committed = node.commit_physical(to_commit_vmem);

    // Keep track of the committed amount
    allocation->set_committed_capacity(committed);

    if (committed != to_commit) {
      allocation->set_commit_failed();

      // Free physical segments for part that we failed to commit
      const ZVirtualMemory non_committed_vmem = to_commit_vmem.last_part(committed);
      node.free_physical(non_committed_vmem);

      // A commit failed, skip the commit for all remaining partial allocations
      return false;
    }
  }

  return true;
}

void ZPageAllocator::map_memory_multi_node(ZMultiNodeAllocation* multi_node_allocation, const ZVirtualMemory& vmem) {
  // Node associated with the final virtual memory
  ZAllocNode& vmem_node = node_from_vmem(vmem);

  size_t offset = 0;
  ZArrayIterator<ZMemoryAllocation*> iter(multi_node_allocation->allocations());
  for (ZMemoryAllocation* allocation; iter.next(&allocation); offset += allocation->size()) {
    // Partition off partial allocation's memory range
    const ZVirtualMemory to_vmem = vmem.partition(offset, allocation->size());

    const uint32_t numa_id = allocation->numa_id();
    ZAllocNode& original_node = node_from_numa_id(numa_id);
    ZArray<ZVirtualMemory>* const partial_vmems = allocation->partial_vmems();

    // Unmap original vmems
    while (!partial_vmems->is_empty()) {
      const ZVirtualMemory to_unmap = partial_vmems->pop();
      original_node.unmap_virtual(to_unmap);
      original_node.free_virtual(to_unmap);
    }

    // Sort physical segments
    sort_segments_physical(to_vmem);

    // Map the partial_allocation to partial_vmem
    vmem_node.map_virtual_to_physical(to_vmem);
  }
}

void ZPageAllocator::cleanup_failed_commit_multi_node(ZMultiNodeAllocation* multi_node_allocation, const ZVirtualMemory& vmem) {
  // Node associated with the final virtual memory
  ZAllocNode& vmem_node = node_from_vmem(vmem);

  size_t offset = 0;
  ZArrayIterator<ZMemoryAllocation*> iter(multi_node_allocation->allocations());
  for (ZMemoryAllocation* allocation; iter.next(&allocation); offset += allocation->size()) {
    const size_t committed = allocation->committed_capacity();

    if (committed == 0) {
      // Nothing committed, nothing to cleanup
      continue;
    }

    // Partition off the partial allocation's memory range
    const ZVirtualMemory partial_vmem = vmem.partition(offset, allocation->size());

    // Remove the harvested part
    const ZVirtualMemory non_harvest_vmem = partial_vmem.last_part(allocation->harvested());

    // Committed part
    const ZVirtualMemory committed_vmem = non_harvest_vmem.first_part(committed);

    const uint32_t numa_id = allocation->numa_id();
    ZAllocNode& node = node_from_numa_id(numa_id);
    ZArray<ZVirtualMemory>* const partial_vmems = allocation->partial_vmems();

    // Keep track of the start index
    const int start_index = partial_vmems->length();

    // Try to claim virtual memory for the committed part
    const size_t claimed_virtual = node.claim_virtual(committed, partial_vmems);

    if (claimed_virtual != committed) {
      // We failed to claim enough virtual memory to map all our committed memory.
      //
      // We currently have no facility to track committed but unmapped memory,
      // so this path uncommits the unmappable memory here.
      //
      // Note that this failure path is only present in multi-node allocations.
      // The single-node allocations claims virtual memory before it tries to
      // commit memory.

      const ZVirtualMemory unmappable = committed_vmem.last_part(claimed_virtual);
      vmem_node.uncommit_physical(unmappable);
      vmem_node.free_physical(unmappable);
    }

    // Associate and map the physical memory with the partial vmems

    size_t claimed_offset = 0;
    ZArrayIterator<ZVirtualMemory> partial_iter(partial_vmems, start_index);
    for (ZVirtualMemory partial_vmem; partial_iter.next(&partial_vmem); claimed_offset += partial_vmem.size()) {
      const ZVirtualMemory from = non_harvest_vmem.partition(claimed_offset, partial_vmem.size());

      // Copy physical mappings
      copy_physical_segments(partial_vmem.start(), from);

      // Map memory
      node.map_virtual_to_physical(partial_vmem);
    }

    assert(claimed_offset == claimed_virtual, "all memory should be accounted for");
  }

  // Free the unused virtual memory
  vmem_node.free_virtual(vmem);
}

bool ZPageAllocator::commit_and_map_memory_multi_node(ZMultiNodeAllocation* multi_node_allocation, const ZVirtualMemory& vmem) {
  if (commit_memory_multi_node(multi_node_allocation, vmem)) {
    // Commit successful
    map_memory_multi_node(multi_node_allocation, vmem);

    return true;
  }

  // Commit failed
  cleanup_failed_commit_multi_node(multi_node_allocation, vmem);

  return false;
}

bool ZPageAllocator::commit_and_map_memory_single_node(ZSingleNodeAllocation* single_node_allocation, const ZVirtualMemory& vmem) {
  ZMemoryAllocation* const allocation = single_node_allocation->allocation();
  const uint32_t numa_id = allocation->numa_id();
  ZAllocNode& node = node_from_numa_id(numa_id);

  return node.commit_and_map_memory(allocation, vmem);
}

bool ZPageAllocator::commit_and_map_memory(ZPageAllocation* allocation, const ZVirtualMemory& vmem) {
  assert(allocation->size() == vmem.size(), "vmem should be the final entry");

  if (allocation->is_multi_node()) {
    return commit_and_map_memory_multi_node(allocation->multi_node_allocation(), vmem);
  } else {
    return commit_and_map_memory_single_node(allocation->single_node_allocation(), vmem);
  }
}

ZPage* ZPageAllocator::alloc_page_inner(ZPageAllocation* allocation) {
retry:

  // Claim the capacity needed for this allocation.
  //
  // The claimed capacity comes from memory already mapped in the cache, or
  // from increasing the capacity. The increased capacity allows us to allocate
  // physical memory from the underlying memory manager later on.
  //
  // Note that this call might block in a safepoint if the non-blocking flag is
  // not set.
  if (!claim_capacity_or_stall(allocation)) {
    // Out of memory
    return nullptr;
  }

  // If the entire claimed capacity came from claiming a single vmem from the
  // mapped cache then the allocation has been satisfied and we are done.
  const ZVirtualMemory cached_vmem = satisfied_from_cache_vmem(allocation);
  if (!cached_vmem.is_null()) {
    return new ZPage(allocation->type(), cached_vmem);
  }

  // We couldn't find a satisfying vmem in the cache, so we need to build one.

  // Claim virtual memory, either from remapping harvested vmems from the
  // mapped cache or by claiming it straight from the virtual manager.
  const ZVirtualMemory vmem = claim_virtual_memory(allocation);
  if (vmem.is_null()) {
    log_error(gc)("Out of address space");
    free_after_alloc_page_failed(allocation);
    return nullptr;
  }

  // Claim physical memory for the increased capacity. The previous claiming of
  // capacity guarantees that this will succeed.
  claim_physical_for_increased_capacity(allocation, vmem);

  // Commit memory for the increased capacity and map the entire vmem.
  if (!commit_and_map_memory(allocation, vmem)) {
    free_after_alloc_page_failed(allocation);
    goto retry;
  }

  // We need to keep track of the physical memory's original alloc nodes
  ZMultiNodeTracker* const tracker = allocation->is_multi_node()
      ? ZMultiNodeTracker::create(allocation->multi_node_allocation(), vmem)
      : nullptr;

  return new ZPage(allocation->type(), vmem, tracker);
}

void ZPageAllocator::increase_used_generation(const ZMemoryAllocation* allocation, ZGenerationId id) {
  const uint32_t numa_id = allocation->numa_id();
  const size_t size = allocation->size();
  _alloc_nodes.get(numa_id).increase_used_generation(id, size);
}

void ZPageAllocator::increase_used_generation_multi_node(const ZMultiNodeAllocation* multi_node_allocation, ZGenerationId id) {
  for (ZMemoryAllocation* allocation : *multi_node_allocation->allocations()) {
    increase_used_generation(allocation, id);
  }
}

void ZPageAllocator::increase_used_generation_single_node(const ZSingleNodeAllocation* single_mode_allocation, ZGenerationId id) {
  increase_used_generation(single_mode_allocation->allocation(), id);
}

void ZPageAllocator::increase_used_generation(const ZPageAllocation* allocation, ZGenerationId id) {
  if (allocation->is_multi_node()) {
    increase_used_generation_multi_node(allocation->multi_node_allocation(), id);
  } else {
    increase_used_generation_single_node(allocation->single_node_allocation(), id);
  }
}

void ZPageAllocator::alloc_page_age_update(ZPageAllocation* allocation, ZPage* page, ZPageAge age) {
  // The generation's used is tracked here when the page is handed out
  // to the allocating thread. The overall heap "used" is tracked in
  // the lower-level allocation code.
  const ZGenerationId id = age == ZPageAge::old ? ZGenerationId::old : ZGenerationId::young;

  increase_used_generation(allocation, id);

  // Reset page. This updates the page's sequence number and must
  // be done after we potentially blocked in a safepoint (stalled)
  // where the global sequence number was updated.
  page->reset(age);
  if (age == ZPageAge::old) {
    page->remset_alloc();
  }
}

ZPage* ZPageAllocator::alloc_page(ZPageType type, size_t size, ZAllocationFlags flags, ZPageAge age) {
  EventZPageAllocation event;

  ZPageAllocation allocation(type, size, flags);
  ZPage* const page = alloc_page_inner(&allocation);
  if (page == nullptr) {
    return nullptr;
  }

  alloc_page_age_update(&allocation, page, age);

  // Update allocation statistics. Exclude gc relocations to avoid
  // artificial inflation of the allocation rate during relocation.
  if (!flags.gc_relocation() && is_init_completed()) {
    // Note that there are two allocation rate counters, which have
    // different purposes and are sampled at different frequencies.
    ZStatInc(ZCounterMutatorAllocationRate, size);
    ZStatMutatorAllocRate::sample_allocation(size);
  }

  const ZPageAllocationStats stats = allocation.stats();
  const int num_harvested_vmems = stats._num_harvested_vmems;
  const size_t harvested = stats._total_harvested;
  const size_t committed = stats._total_committed_capacity;

  if (harvested > 0) {
    log_debug(gc, heap)("Mapped Cache Harvest: %zuM from %d ranges", harvested / M, num_harvested_vmems);
  }

  // Send event
  event.commit((u8)type, size, harvested, committed,
               (unsigned int)count_segments_physical(page->virtual_memory()), flags.non_blocking());

  return page;
}

void ZPageAllocator::safe_destroy_page(ZPage* page) {
  // Destroy page safely
  _safe_destroy.schedule_delete(page);
}

void ZPageAllocator::satisfy_stalled() {
  for (;;) {
    ZPageAllocation* const allocation = _stalled.first();
    if (allocation == nullptr) {
      // Allocation queue is empty
      return;
    }

    if (!claim_capacity(allocation)) {
      // Allocation could not be satisfied, give up
      return;
    }

    // Allocation succeeded, dequeue and satisfy allocation request.
    // Note that we must dequeue the allocation request first, since
    // it will immediately be deallocated once it has been satisfied.
    _stalled.remove(allocation);
    allocation->satisfy(true);
  }
}

void ZPageAllocator::prepare_memory_for_free(ZPage* page, ZArray<ZVirtualMemory>* entries, bool allow_defragment) {
  // Extract memory and destroy page
  const ZVirtualMemory vmem = page->virtual_memory();
  const ZPageType page_type = page->type();
  safe_destroy_page(page);

  // Perhaps remap vmem
  if (page_type == ZPageType::large && allow_defragment) {
    remap_and_defragment(vmem, entries);
  } else {
    entries->append(vmem);
  }
}

void ZPageAllocator::free_page_multi_node(ZPage* page) {
  assert(page->is_multi_node(), "only used for multi-node pages");
  ZMultiNodeTracker::free_and_destroy(this, page);
}

void ZPageAllocator::free_page(ZPage* page, bool allow_defragment) {
  if (page->is_multi_node()) {
    // Multi-node is handled separately, multi-node allocations are always
    // effectively defragmented
    free_page_multi_node(page);
    return;
  }

  ZArray<ZVirtualMemory> to_cache;

  ZGenerationId id = page->generation_id();
  ZAllocNode& node = node_from_vmem(page->virtual_memory());
  prepare_memory_for_free(page, &to_cache, allow_defragment);

  ZLocker<ZLock> locker(&_lock);

  ZArrayIterator<ZVirtualMemory> iter(&to_cache);
  for (ZVirtualMemory vmem; iter.next(&vmem);) {
    // Update used statistics and cache memory
    node.decrease_used(vmem.size());
    node.decrease_used_generation(id, vmem.size());
    node._cache.insert(vmem);
  }

  // Try satisfy stalled allocations
  satisfy_stalled();
}

void ZPageAllocator::free_pages(const ZArray<ZPage*>* pages) {
  ZArray<ZVirtualMemory> to_cache;

  // All pages belong to the same generation, so either only young or old.
  const ZGenerationId gen_id = pages->first()->generation_id();

  // Prepare memory from pages to be cached before taking the lock
  ZArrayIterator<ZPage*> pages_iter(pages);
  for (ZPage* page; pages_iter.next(&page);) {
    if (page->is_multi_node()) {
      // Multi numa is handled separately
      free_page_multi_node(page);
      continue;
    }

    prepare_memory_for_free(page, &to_cache, true /* allow_defragment */);
  }

  const uint32_t numa_nodes = ZNUMA::count();
  ZLocker<ZLock> locker(&_lock);

  // Insert vmems to the cache
  ZArrayIterator<ZVirtualMemory> iter(&to_cache);
  for (ZVirtualMemory vmem; iter.next(&vmem);) {
    ZAllocNode& node = node_from_vmem(vmem);
    const size_t size = vmem.size();

    // Reinsert vmems
    node.cache()->insert(vmem);

    // Update accounting
    node.decrease_used(size);
    node.decrease_used_generation(gen_id, size);
  }

  // Try satisfy stalled allocations
  satisfy_stalled();
}

void ZPageAllocator::free_memory_alloc_failed(ZMemoryAllocation* allocation) {
  const uint32_t numa_id = allocation->numa_id();
  ZAllocNode& node = node_from_numa_id(numa_id);

  node.free_memory_alloc_failed(allocation);
}

void ZPageAllocator::free_memory_alloc_failed_multi_node(ZMultiNodeAllocation* multi_node_allocation) {
  for (ZMemoryAllocation* allocation : *multi_node_allocation->allocations()) {
    free_memory_alloc_failed(allocation);
  }
}

void ZPageAllocator::free_memory_alloc_failed_single_node(ZSingleNodeAllocation* single_node_allocation) {
  free_memory_alloc_failed(single_node_allocation->allocation());
}

void ZPageAllocator::free_memory_alloc_failed(ZPageAllocation* allocation) {
  if (allocation->is_multi_node()) {
    free_memory_alloc_failed_multi_node(allocation->multi_node_allocation());
  } else {
    free_memory_alloc_failed_single_node(allocation->single_node_allocation());
  }
}

void ZPageAllocator::free_after_alloc_page_failed(ZPageAllocation* allocation) {
  ZLocker<ZLock> locker(&_lock);

  free_memory_alloc_failed(allocation);

  // Reset allocation for a potential retry
  allocation->reset_for_retry();

  // Try satisfy stalled allocations
  satisfy_stalled();
}

void ZPageAllocator::enable_safe_destroy() const {
  _safe_destroy.enable_deferred_delete();
}

void ZPageAllocator::disable_safe_destroy() const {
  _safe_destroy.disable_deferred_delete();
}

static bool has_alloc_seen_young(const ZPageAllocation* allocation) {
  return allocation->young_seqnum() != ZGeneration::young()->seqnum();
}

static bool has_alloc_seen_old(const ZPageAllocation* allocation) {
  return allocation->old_seqnum() != ZGeneration::old()->seqnum();
}

bool ZPageAllocator::is_alloc_stalling() const {
  ZLocker<ZLock> locker(&_lock);
  return _stalled.first() != nullptr;
}

bool ZPageAllocator::is_alloc_stalling_for_old() const {
  ZLocker<ZLock> locker(&_lock);

  ZPageAllocation* const allocation = _stalled.first();
  if (allocation == nullptr) {
    // No stalled allocations
    return false;
  }

  return has_alloc_seen_young(allocation) && !has_alloc_seen_old(allocation);
}

void ZPageAllocator::notify_out_of_memory() {
  // Fail allocation requests that were enqueued before the last major GC started
  for (ZPageAllocation* allocation = _stalled.first(); allocation != nullptr; allocation = _stalled.first()) {
    if (!has_alloc_seen_old(allocation)) {
      // Not out of memory, keep remaining allocation requests enqueued
      return;
    }

    // Out of memory, dequeue and fail allocation request
    _stalled.remove(allocation);
    allocation->satisfy(false);
  }
}

void ZPageAllocator::restart_gc() const {
  ZPageAllocation* const allocation = _stalled.first();
  if (allocation == nullptr) {
    // No stalled allocations
    return;
  }

  if (!has_alloc_seen_young(allocation)) {
    // Start asynchronous minor GC, keep allocation requests enqueued
    const ZDriverRequest request(GCCause::_z_allocation_stall, ZYoungGCThreads, 0);
    ZDriver::minor()->collect(request);
  } else {
    // Start asynchronous major GC, keep allocation requests enqueued
    const ZDriverRequest request(GCCause::_z_allocation_stall, ZYoungGCThreads, ZOldGCThreads);
    ZDriver::major()->collect(request);
  }
}

void ZPageAllocator::handle_alloc_stalling_for_young() {
  ZLocker<ZLock> locker(&_lock);
  restart_gc();
}

void ZPageAllocator::handle_alloc_stalling_for_old(bool cleared_all_soft_refs) {
  ZLocker<ZLock> locker(&_lock);
  if (cleared_all_soft_refs) {
    notify_out_of_memory();
  }
  restart_gc();
}

void ZPageAllocator::threads_do(ThreadClosure* tc) const {
  ZPerNUMAConstIterator<ZAllocNode> iter = alloc_node_iterator();
  for (const ZAllocNode* node; iter.next(&node);) {
    node->threads_do(tc);
  }
}

void ZPageAllocator::print_on(outputStream* st) const {
  ZLocker<ZLock> lock(&_lock);
  print_on_inner(st);
}

void ZPageAllocator::print_extended_on_error(outputStream* st) const {
  if(!_lock.try_lock()) {
    // We can't print without taking the lock since printing the contents of
    // the cache requires iterating over the nodes in the cache's tree, which
    // is not thread-safe.
    return;
  }

  // Print each node's cache content
  st->print_cr("ZMappedCache:");
  ZPerNUMAConstIterator<ZAllocNode> iter = alloc_node_iterator();
  for (const ZAllocNode* node; iter.next(&node);) {
    node->print_extended_on_error(st);
  }

  _lock.unlock();
}

void ZPageAllocator::print_on_error(outputStream* st) const {
  if(!_lock.try_lock()) {
    // Print information even though we have not successfully taken the lock.
    // This is thread-safe, but may produce inconsistent results.
    print_on_inner(st);
    return;
  }

  print_on_inner(st);

  _lock.unlock();
}

void ZPageAllocator::print_on_inner(outputStream* st) const {
  // Print total usage
  st->print_cr(" ZHeap           used %zuM, capacity %zuM, max capacity %zuM",
               used() / M, capacity() / M, max_capacity() / M);

  // Print per-node
  ZPerNUMAConstIterator<ZAllocNode> iter = alloc_node_iterator();
  for (const ZAllocNode* node; iter.next(&node);) {
    node->print_on(st);
  }
}

ZPerNUMAConstIterator<ZAllocNode> ZPageAllocator::alloc_node_iterator() const {
  return ZPerNUMAConstIterator<ZAllocNode>(&_alloc_nodes);
}

ZPerNUMAIterator<ZAllocNode> ZPageAllocator::alloc_node_iterator() {
  return ZPerNUMAIterator<ZAllocNode>(&_alloc_nodes);
}
