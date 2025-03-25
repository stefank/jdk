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

#include "gc/shared/gc_globals.hpp"
#include "gc/shared/gcLogPrecious.hpp"
#include "gc/z/zAddress.inline.hpp"
#include "gc/z/zAddressSpaceLimit.hpp"
#include "gc/z/zArray.hpp"
#include "gc/z/zGlobals.hpp"
#include "gc/z/zInitialize.hpp"
#include "gc/z/zMemory.inline.hpp"
#include "gc/z/zNMT.hpp"
#include "gc/z/zNUMA.inline.hpp"
#include "gc/z/zValue.inline.hpp"
#include "utilities/align.hpp"
#include "utilities/debug.hpp"

ZVirtualMemoryReserver::ZVirtualMemoryReserver(size_t size)
  : _virtual_memory_reservation(),
    _reserved(reserve(size)) {}

void ZVirtualMemoryReserver::unreserve() {
  for (ZVirtualMemory vmem; _virtual_memory_reservation.unregister_first(&vmem);) {
    const zaddress_unsafe addr = ZOffset::address_unsafe(vmem.start());

     // Unreserve address space
     pd_unreserve(addr, vmem.size());
  }
}

bool ZVirtualMemoryReserver::is_empty() const {
  return _virtual_memory_reservation.is_empty();
}

bool ZVirtualMemoryReserver::is_contiguous() const {
  return _virtual_memory_reservation.is_contiguous();
}

size_t ZVirtualMemoryReserver::reserved() const {
  return _reserved;
}

void ZVirtualMemoryReserver::initialize_node(ZMemoryManager* node, size_t size) {
  assert(node->is_empty(), "Should be empty when initializing");

  // Registers the Windows callbacks
  pd_register_callbacks(node);

  _virtual_memory_reservation.transfer_from_low(node, size);

  // Set the limits according to the virtual memory given to this node
  node->anchor_limits();
}

void ZVirtualMemoryManager::initialize_nodes(ZVirtualMemoryReserver* reserver, size_t size_for_nodes) {
  precond(is_aligned(size_for_nodes, ZGranuleSize));

  // If the capacity consist of less granules than the number of nodes some
  // nodes will be empty. Distribute these shares on the none empty nodes.
  const uint32_t first_empty_numa_id = MIN2(static_cast<uint32_t>(size_for_nodes >> ZGranuleSizeShift), ZNUMA::count());
  const uint32_t ignore_count = ZNUMA::count() - first_empty_numa_id;

  // Install reserved memory into manager(s)
  uint32_t numa_id;
  ZPerNUMAIterator<ZMemoryManager> iter(&_nodes);
  for (ZMemoryManager* node; iter.next(&node, &numa_id);) {
    if (numa_id == first_empty_numa_id) {
      break;
    }

    // Calculate how much reserved memory this node gets
    const size_t reserved_for_node = ZNUMA::calculate_share(numa_id, size_for_nodes, ZGranuleSize, ignore_count);

    // Transfer reserved memory
    reserver->initialize_node(node, reserved_for_node);
  }
}

#ifdef ASSERT
size_t ZVirtualMemoryReserver::force_reserve_discontiguous(size_t size) {
  const size_t min_range = calculate_min_range(size);
  const size_t max_range = MAX2(align_down(size / ZForceDiscontiguousHeapReservations, ZGranuleSize), min_range);
  size_t reserved = 0;

  // Try to reserve ZForceDiscontiguousHeapReservations number of virtual memory
  // ranges. Starting with higher addresses.
  uintptr_t end = ZAddressOffsetMax;
  while (reserved < size && end >= max_range) {
    const size_t remaining = size - reserved;
    const size_t reserve_size = MIN2(max_range, remaining);
    const uintptr_t reserve_start = end - reserve_size;

    if (reserve_contiguous(to_zoffset(reserve_start), reserve_size)) {
      reserved += reserve_size;
    }

    end -= reserve_size * 2;
  }

  // If (reserved < size) attempt to reserve the rest via normal divide and conquer
  uintptr_t start = 0;
  while (reserved < size && start < ZAddressOffsetMax) {
    const size_t remaining = MIN2(size - reserved, ZAddressOffsetMax - start);
    reserved += reserve_discontiguous(to_zoffset(start), remaining, min_range);
    start += remaining;
  }

  return reserved;
}
#endif

size_t ZVirtualMemoryReserver::reserve_discontiguous(zoffset start, size_t size, size_t min_range) {
  if (size < min_range) {
    // Too small
    return 0;
  }

  assert(is_aligned(size, ZGranuleSize), "Misaligned");

  if (reserve_contiguous(start, size)) {
    return size;
  }

  const size_t half = size / 2;
  if (half < min_range) {
    // Too small
    return 0;
  }

  // Divide and conquer
  const size_t first_part = align_down(half, ZGranuleSize);
  const size_t second_part = size - first_part;
  const size_t first_size = reserve_discontiguous(start, first_part, min_range);
  const size_t second_size = reserve_discontiguous(start + first_part, second_part, min_range);
  return first_size + second_size;
}

size_t ZVirtualMemoryReserver::calculate_min_range(size_t size) {
  // Don't try to reserve address ranges smaller than 1% of the requested size.
  // This avoids an explosion of reservation attempts in case large parts of the
  // address space is already occupied.
  return align_up(size / ZMaxVirtualReservations, ZGranuleSize);
}

size_t ZVirtualMemoryReserver::reserve_discontiguous(size_t size) {
  const size_t min_range = calculate_min_range(size);
  uintptr_t start = 0;
  size_t reserved = 0;

  // Reserve size somewhere between [0, ZAddressOffsetMax)
  while (reserved < size && start < ZAddressOffsetMax) {
    const size_t remaining = MIN2(size - reserved, ZAddressOffsetMax - start);
    reserved += reserve_discontiguous(to_zoffset(start), remaining, min_range);
    start += remaining;
  }

  return reserved;
}

bool ZVirtualMemoryReserver::reserve_contiguous(zoffset start, size_t size) {
  assert(is_aligned(size, ZGranuleSize), "Must be granule aligned 0x%zx", size);

  // Reserve address views
  const zaddress_unsafe addr = ZOffset::address_unsafe(start);

  // Reserve address space
  if (!pd_reserve(addr, size)) {
    return false;
  }

  // Register address views with native memory tracker
  ZNMT::reserve(addr, size);

  // Register the memory reservation
  _virtual_memory_reservation.register_range({start, size});

  return true;
}

bool ZVirtualMemoryReserver::reserve_contiguous(size_t size) {
  // Allow at most 8192 attempts spread evenly across [0, ZAddressOffsetMax)
  const size_t unused = ZAddressOffsetMax - size;
  const size_t increment = MAX2(align_up(unused / 8192, ZGranuleSize), ZGranuleSize);

  for (uintptr_t start = 0; start + size <= ZAddressOffsetMax; start += increment) {
    if (reserve_contiguous(to_zoffset(start), size)) {
      // Success
      return true;
    }
  }

  // Failed
  return false;
}

size_t ZVirtualMemoryReserver::reserve(size_t size) {
  // Initialize platform specific parts before reserving address space
  pd_initialize_before_reserve();

  pd_register_callbacks(&_virtual_memory_reservation);

  // Reserve address space

#ifdef ASSERT
  if (ZForceDiscontiguousHeapReservations > 0) {
    return force_reserve_discontiguous(size);
  }
#endif

  // Prefer a contiguous address space
  if (reserve_contiguous(size)) {
    return size;
  }

  // Fall back to a discontiguous address space
  return reserve_discontiguous(size);
}

ZVirtualMemoryManager::ZVirtualMemoryManager(size_t max_capacity)
  : _nodes(),
    _multi_node(),
    _initialized(false) {

  assert(max_capacity <= ZAddressOffsetMax, "Too large max_capacity");

  const size_t limit = MIN2(ZAddressOffsetMax, ZAddressSpaceLimit::heap());

  const size_t desired_for_nodes = max_capacity * ZVirtualToPhysicalRatio;
  const size_t desired_for_multi_node = ZNUMA::count() > 1 ? max_capacity : 0;

  const size_t desired = desired_for_nodes + desired_for_multi_node;
  const size_t requested = desired <= limit
      ? desired
      : MIN2(desired_for_nodes, limit);

  // Reserve virtual memory for the heap
  ZVirtualMemoryReserver reserver(requested);

  const size_t reserved = reserver.reserved();
  const bool is_contiguous = reserver.is_contiguous();

  if (reserved < max_capacity) {
    ZInitialize::error_d("Failed to reserve " EXACTFMT " address space for Java heap", EXACTFMTARGS(max_capacity));
    return;
  }

  const size_t size_for_nodes = MIN2(reserved, desired_for_nodes);

  // Divide size_for_nodes virtual memory over the NUMA nodes
  initialize_nodes(&reserver, size_for_nodes);

  if (desired_for_multi_node > 0 && reserved == desired) {
    // Enough left to setup the multi-node memory reservation
    reserver.initialize_node(&_multi_node, max_capacity);
  } else {
    // Failed to reserve enough memory for multi-node, unreserve unused memory
    reserver.unreserve();
  }

  assert(reserver.is_empty(), "Must have handled all reserved memory");

  log_info_p(gc, init)("Address Space Type: %s/%s/%s",
                       (is_contiguous ? "Contiguous" : "Discontiguous"),
                       (limit == ZAddressOffsetMax ? "Unrestricted" : "Restricted"),
                       (reserved >= desired_for_nodes ? "Complete" : "Degraded"));
  log_info_p(gc, init)("Address Space Size: %zuM", reserved / M);

  // Successfully initialized
  _initialized = true;
}

bool ZVirtualMemoryManager::is_initialized() const {
  return _initialized;
}

bool ZVirtualMemoryManager::is_multi_node_enabled() const {
  return !_multi_node.is_empty();
}

bool ZVirtualMemoryManager::is_in_multi_node(const ZVirtualMemory& vmem) const {
  return _multi_node.limits_contain(vmem);
}

uint32_t ZVirtualMemoryManager::get_numa_id(const ZVirtualMemory& vmem) const {
  const uint32_t numa_nodes = ZNUMA::count();
  for (uint32_t numa_id = 0; numa_id < numa_nodes; numa_id++) {
    if (_nodes.get(numa_id).limits_contain(vmem)) {
      return numa_id;
    }
  }

  ShouldNotReachHere();
}

zoffset ZVirtualMemoryManager::lowest_available_address(uint32_t numa_id) const {
  return _nodes.get(numa_id).peek_low_address();
}

void ZVirtualMemoryManager::insert(const ZVirtualMemory& vmem, uint32_t numa_id) {
  assert(numa_id == get_numa_id(vmem), "wrong numa_id for vmem");
  _nodes.get(numa_id).insert(vmem);
}

void ZVirtualMemoryManager::insert_multi_node(const ZVirtualMemory& vmem) {
  _multi_node.insert(vmem);
}

size_t ZVirtualMemoryManager::remove_from_low_many_at_most(size_t size, uint32_t numa_id, ZArray<ZVirtualMemory>* vmems_out) {
  return _nodes.get(numa_id).remove_from_low_many_at_most(size, vmems_out);
}

ZVirtualMemory ZVirtualMemoryManager::remove_from_low(size_t size, uint32_t numa_id) {
  return _nodes.get(numa_id).remove_from_low(size);
}

ZVirtualMemory ZVirtualMemoryManager::remove_from_low_multi_node(size_t size) {
  return _multi_node.remove_from_low(size);
}

void ZVirtualMemoryManager::insert_and_remove_from_low_many(const ZVirtualMemory& vmem, uint32_t numa_id, ZArray<ZVirtualMemory>* vmems_out) {
  _nodes.get(numa_id).insert_and_remove_from_low_many(vmem, vmems_out);
}

ZVirtualMemory ZVirtualMemoryManager::insert_and_remove_from_low_exact_or_many(size_t size, uint32_t numa_id, ZArray<ZVirtualMemory>* vmems_in_out) {
  return _nodes.get(numa_id).insert_and_remove_from_low_exact_or_many(size, vmems_in_out);
}
