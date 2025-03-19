/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_GC_Z_ZPAGEALLOCATOR_HPP
#define SHARE_GC_Z_ZPAGEALLOCATOR_HPP

#include "gc/z/zAddress.hpp"
#include "gc/z/zAllocationFlags.hpp"
#include "gc/z/zArray.hpp"
#include "gc/z/zGranuleMap.hpp"
#include "gc/z/zList.hpp"
#include "gc/z/zLock.hpp"
#include "gc/z/zMappedCache.hpp"
#include "gc/z/zMemory.hpp"
#include "gc/z/zPage.hpp"
#include "gc/z/zPageAge.hpp"
#include "gc/z/zPageType.hpp"
#include "gc/z/zPhysicalMemoryManager.hpp"
#include "gc/z/zSafeDelete.hpp"
#include "gc/z/zUncommitter.hpp"
#include "gc/z/zValue.hpp"
#include "gc/z/zVirtualMemoryManager.hpp"

class ThreadClosure;
class ZGeneration;
class ZMemoryAllocation;
class ZMultiNodeAllocation;
class ZPageAllocation;
class ZPageAllocator;
class ZPageAllocatorStats;
class ZSegmentStash;
class ZSingleNodeAllocation;
class ZWorkers;

class ZAllocNode {
  friend class VMStructs;
  friend class ZPageAllocator;

private:
  ZPageAllocator* const      _page_allocator;
  ZMappedCache               _cache;
  ZUncommitter               _uncommitter;
  const size_t               _min_capacity;
  const size_t               _initial_capacity;
  const size_t               _max_capacity;
  size_t                     _current_max_capacity;
  volatile size_t            _capacity;
  volatile size_t            _claimed;
  volatile size_t            _used;
  size_t                     _used_generations[2];
  struct {
    size_t                   _used_high;
    size_t                   _used_low;
  } _collection_stats[2];
  double                     _last_commit;
  double                     _last_uncommit;
  size_t                     _to_uncommit;
  const uint32_t             _numa_id;

  const ZVirtualMemoryManager& virtual_memory_manager() const;
  ZVirtualMemoryManager& virtual_memory_manager();

  const ZPhysicalMemoryManager& physical_memory_manager() const;
  ZPhysicalMemoryManager& physical_memory_manager();

  const ZGranuleMap<zbacking_index>& physical_mappings() const;
  ZGranuleMap<zbacking_index>& physical_mappings();

  const zbacking_index* physical_mappings_addr(const ZVirtualMemory& vmem) const;
  zbacking_index* physical_mappings_addr(const ZVirtualMemory& vmem);

  void verify_virtual_memory_association(const ZVirtualMemory& vmem) const;
  void verify_virtual_memory_association(const ZArray<ZVirtualMemory>* vmems) const;
  void verify_memory_allocation_association(const ZMemoryAllocation* allocation) const;

public:
  ZAllocNode(uint32_t numa_id, ZPageAllocator* page_allocator);

  size_t available_capacity() const;

  size_t increase_capacity(size_t size);
  void decrease_capacity(size_t size, bool set_max_capacity);

  void increase_used(size_t size);
  void decrease_used(size_t size);

  void increase_used_generation(ZGenerationId id, size_t size);
  void decrease_used_generation(ZGenerationId id, size_t size);

  void reset_statistics(ZGenerationId id);

  void claim_mapped_or_increase_capacity(ZMemoryAllocation* allocation);
  bool claim_physical(ZMemoryAllocation* allocation);

  void promote_used(size_t size);

  ZMappedCache* cache();

  uint32_t numa_id() const;

  size_t uncommit(uint64_t* timeout);

  const ZUncommitter& uncommitter() const;
  ZUncommitter& uncommitter();

  void threads_do(ThreadClosure* tc) const;

  void alloc_physical(const ZVirtualMemory& vmem);
  void free_physical(const ZVirtualMemory& vmem);
  size_t commit_physical(const ZVirtualMemory& vmem);
  size_t uncommit_physical(const ZVirtualMemory& vmem);

  void map_virtual_to_physical(const ZVirtualMemory& vmem);
  void unmap_virtual(const ZVirtualMemory& vmem);

  ZVirtualMemory claim_virtual(size_t size, bool force_low_address);
  size_t claim_virtual(size_t size, ZArray<ZVirtualMemory>* vmems_out);
  void free_virtual(const ZVirtualMemory& vmem);

  void shuffle_virtual(const ZVirtualMemory& vmem, ZArray<ZVirtualMemory>* vmems_out);
  ZVirtualMemory shuffle_virtual(size_t size, ZArray<ZVirtualMemory>* vmems_in_out);

  bool prime(ZWorkers* workers, size_t size);

  void remap_harvested_and_claim_virtual(ZMemoryAllocation* allocation);
  bool claim_virtual_memory(ZMemoryAllocation* allocation);

  ZVirtualMemory commit_increased_capacity(ZMemoryAllocation* allocation, const ZVirtualMemory& vmem);
  void map_memory(ZMemoryAllocation* allocation, const ZVirtualMemory& vmem);

  bool commit_and_map_memory(ZMemoryAllocation* allocation, const ZVirtualMemory& vmem);
  void free_memory_alloc_failed(ZMemoryAllocation* allocation);
};

class ZPageAllocator {
  friend class VMStructs;
  friend class ZAllocNode;
  friend class ZUncommitter;
  friend class ZMultiNodeTracker;

private:
  mutable ZLock               _lock;
  ZVirtualMemoryManager       _virtual;
  ZPhysicalMemoryManager      _physical;
  ZGranuleMap<zbacking_index> _physical_mappings;
  const size_t                _min_capacity;
  const size_t                _initial_capacity;
  const size_t                _max_capacity;
  ZPerNUMA<ZAllocNode>        _alloc_nodes;
  ZList<ZPageAllocation>      _stalled;
  mutable ZSafeDelete<ZPage>  _safe_destroy;
  bool                        _initialized;

  const ZAllocNode& node_from_numa_id(uint32_t numa_id) const;
  ZAllocNode& node_from_numa_id(uint32_t numa_id);
  ZAllocNode& node_from_vmem(const ZVirtualMemory& vmem);

  size_t count_segments_physical(const ZVirtualMemory& vmem);
  void sort_segments_physical(const ZVirtualMemory& vmem);

  void remap_and_defragment(const ZVirtualMemory& vmem, ZArray<ZVirtualMemory>* entries);
  void prepare_memory_for_free(ZPage* page, ZArray<ZVirtualMemory>* entries, bool allow_defragment);

  bool alloc_page_stall(ZPageAllocation* allocation);

  size_t sum_available_capacity() const;

  bool claim_physical_multi_node(ZMultiNodeAllocation* multi_node_allocation, uint32_t start_node);
  bool claim_physical_single_node(ZSingleNodeAllocation* single_node_allocation, uint32_t numa_id);
  bool claim_physical(ZPageAllocation* allocation);

  bool claim_physical_or_stall(ZPageAllocation* allocation);

  bool is_alloc_satisfied_multi_node(const ZMultiNodeAllocation* multi_node_allocation) const;
  bool is_alloc_satisfied_single_node(const ZSingleNodeAllocation* single_node_allocation) const;
  bool is_alloc_satisfied(const ZPageAllocation* allocation) const;

  void copy_physical_segments(zoffset to, const ZVirtualMemory& from);
  void copy_claimed_physical_multi_node(ZMultiNodeAllocation* multi_node_allocation, const ZVirtualMemory& vmem);

  bool claim_virtual_memory_multi_node(ZMultiNodeAllocation* multi_node_allocation);
  bool claim_virtual_memory_single_node(ZSingleNodeAllocation* single_node_allocation);
  bool claim_virtual_memory(ZPageAllocation* allocation);

  void allocate_remaining_physical(ZMemoryAllocation* allocation, const ZVirtualMemory& vmem);
  void allocate_remaining_physical_multi_node(const ZMultiNodeAllocation* multi_node_allocation, const ZVirtualMemory& vmem);
  void allocate_remaining_physical_single_node(ZSingleNodeAllocation* allocation, const ZVirtualMemory& vmem);
  void allocate_remaining_physical(ZPageAllocation* allocation, const ZVirtualMemory& vmem);

  bool commit_memory_multi_node(ZMultiNodeAllocation* multi_node_allocation, const ZVirtualMemory& vmem);
  void map_memory_multi_node(ZMultiNodeAllocation* multi_node_allocation, const ZVirtualMemory& vmem);
  void cleanup_failed_commit_multi_node(ZMultiNodeAllocation* multi_node_allocation, const ZVirtualMemory& vmem);

  bool commit_and_map_memory_multi_node(ZMultiNodeAllocation* multi_node_allocation, const ZVirtualMemory& vmem);
  bool commit_and_map_memory_single_node(ZSingleNodeAllocation* single_node_allocation, const ZVirtualMemory& vmem);
  bool commit_and_map_memory(ZPageAllocation* allocation, const ZVirtualMemory& vmem);

  ZPage* alloc_page_inner(ZPageAllocation* allocation);

  void increase_used_generation(const ZMemoryAllocation* allocation, ZGenerationId id);
  void increase_used_generation_multi_node(const ZMultiNodeAllocation* multi_node_allocation, ZGenerationId id);
  void increase_used_generation_single_node(const ZSingleNodeAllocation* single_mode_allocation, ZGenerationId id);
  void increase_used_generation(const ZPageAllocation* allocation, ZGenerationId id);

  void alloc_page_age_update(ZPageAllocation* allocation, ZPage* page, ZPageAge age);

  void free_memory_alloc_failed(ZMemoryAllocation* allocation);
  void free_memory_alloc_failed_multi_node(ZMultiNodeAllocation* multi_node_allocation);
  void free_memory_alloc_failed_single_node(ZSingleNodeAllocation* single_node_allocation);
  void free_memory_alloc_failed(ZPageAllocation* allocation);

  void free_after_alloc_page_failed(ZPageAllocation* allocation);

  void satisfy_stalled();

  void notify_out_of_memory();
  void restart_gc() const;

public:
  ZPageAllocator(size_t min_capacity,
                 size_t initial_capacity,
                 size_t soft_max_capacity,
                 size_t max_capacity);

  bool is_initialized() const;

  bool prime_cache(ZWorkers* workers, size_t size);

  size_t initial_capacity() const;
  size_t min_capacity() const;
  size_t max_capacity() const;
  size_t soft_max_capacity() const;
  size_t current_max_capacity() const;
  size_t capacity() const;
  size_t used() const;
  size_t used_generation(ZGenerationId id) const;
  size_t unused() const;

  void promote_used(const ZPage* from, const ZPage* to);

  ZPageAllocatorStats stats(ZGeneration* generation) const;

  void reset_statistics(ZGenerationId id);

  ZPage* alloc_page(ZPageType type, size_t size, ZAllocationFlags flags, ZPageAge age);
  void safe_destroy_page(ZPage* page);
  void free_page_multi_node(ZPage* page);
  void free_page(ZPage* page, bool allow_defragment);
  void free_pages(const ZArray<ZPage*>* pages);

  void enable_safe_destroy() const;
  void disable_safe_destroy() const;

  bool is_alloc_stalling() const;
  bool is_alloc_stalling_for_old() const;
  void handle_alloc_stalling_for_young();
  void handle_alloc_stalling_for_old(bool cleared_soft_refs);

  void threads_do(ThreadClosure* tc) const;

  ZPerNUMAConstIterator<ZAllocNode> alloc_node_iterator() const;
  ZPerNUMAIterator<ZAllocNode> alloc_node_iterator();
};

class ZPageAllocatorStats {
private:
  size_t _min_capacity;
  size_t _max_capacity;
  size_t _soft_max_capacity;
  size_t _freed;
  size_t _promoted;
  size_t _compacted;
  size_t _allocation_stalls;

  size_t _capacity;
  size_t _used;
  size_t _used_high;
  size_t _used_low;
  size_t _used_generation;

public:
  ZPageAllocatorStats(size_t min_capacity,
                      size_t max_capacity,
                      size_t soft_max_capacity,
                      size_t freed,
                      size_t promoted,
                      size_t compacted,
                      size_t allocation_stalls);

  void increment_stats(size_t capacity,
                       size_t used,
                       size_t used_high,
                       size_t used_low,
                       size_t used_generation);

  size_t min_capacity() const;
  size_t max_capacity() const;
  size_t soft_max_capacity() const;
  size_t freed() const;
  size_t promoted() const;
  size_t compacted() const;
  size_t allocation_stalls() const;

  size_t capacity() const;
  size_t used() const;
  size_t used_high() const;
  size_t used_low() const;
  size_t used_generation() const;
};

#endif // SHARE_GC_Z_ZPAGEALLOCATOR_HPP
