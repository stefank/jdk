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

ZVirtualMemoryManager::ZVirtualMemoryManager(size_t max_capacity)
  : _extra_node(),
    _nodes(),
    _vmem_ranges(),
    _reserved_extra_space(false),
    _initialized(false) {

  assert(max_capacity <= ZAddressOffsetMax, "Too large max_capacity");

  // Initialize platform specific parts before reserving address space
  pd_initialize_before_reserve();

  // Reserve address space
  const size_t reserved = reserve(max_capacity);
  if (reserved < max_capacity) {
    ZInitialize::error_d("Failed to reserve enough address space for Java heap");
    return;
  }

  // Initialize platform specific parts after reserving address space
  pd_initialize_after_reserve();

  // Divide the reserved memory over the NUMA nodes
  initialize_nodes(max_capacity, reserved);

  // Successfully initialized
  _initialized = true;
}

void ZVirtualMemoryManager::initialize_nodes(size_t max_capacity, size_t reserved) {
  // If the capacity consist of less granules than the number of nodes some
  // nodes will be empty. Distribute these shares on the none empty nodes.
  const uint32_t first_empty_numa_id = MIN2(static_cast<uint32_t>(max_capacity >> ZGranuleSizeShift), ZNUMA::count());
  const uint32_t ignore_count = ZNUMA::count() - first_empty_numa_id;

  // Extra reservation size, not installed into node manager(s)
  const size_t extra_size = _reserved_extra_space ? max_capacity : 0;

  // Install reserved memory into manager(s)
  uint32_t numa_id;
  ZPerNUMAIterator<ZMemoryManager> iter(&_nodes);
  for (ZMemoryManager* manager; iter.next(&manager, &numa_id);) {
    if (numa_id == first_empty_numa_id) {
      // The remaining nodes are all empty
      break;
    }

    // Calculate how much reserved memory this node gets
    const size_t reserved_for_node = ZNUMA::calculate_share(numa_id, reserved - extra_size, ZGranuleSize, ignore_count);

    // Transfer reserved memory
    _extra_node.transfer_low_address(manager, reserved_for_node);

    // Store the range for the manager
    _vmem_ranges.set(manager->total_range(), numa_id);
  }

  if (_reserved_extra_space) {
    assert(!_extra_node.total_range().is_null(), "must have extra space");
    _extra_space_range = _extra_node.total_range();
  } else {

  assert(_extra_node.total_range().is_null(), "must insert all reserved");
  }
}

#ifdef ASSERT
size_t ZVirtualMemoryManager::force_reserve_discontiguous(size_t size) {
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

size_t ZVirtualMemoryManager::reserve_discontiguous(zoffset start, size_t size, size_t min_range) {
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

size_t ZVirtualMemoryManager::calculate_min_range(size_t size) {
  // Don't try to reserve address ranges smaller than 1% of the requested size.
  // This avoids an explosion of reservation attempts in case large parts of the
  // address space is already occupied.
  return align_up(size / ZMaxVirtualReservations, ZGranuleSize);
}

size_t ZVirtualMemoryManager::reserve_discontiguous(size_t size) {
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

bool ZVirtualMemoryManager::reserve_contiguous(zoffset start, size_t size) {
  assert(is_aligned(size, ZGranuleSize), "Must be granule aligned 0x%zx", size);

  // Reserve address views
  const zaddress_unsafe addr = ZOffset::address_unsafe(start);

  // Reserve address space
  if (!pd_reserve(addr, size)) {
    return false;
  }

  // Register address views with native memory tracker
  ZNMT::reserve(addr, size);

  // Insert the address range in the extra manager.
  // This will later be distributed among the available NUMA nodes.
  _extra_node.insert(start, size);

  return true;
}

bool ZVirtualMemoryManager::reserve_contiguous(size_t size) {
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

size_t ZVirtualMemoryManager::reserve(size_t max_capacity) {
  const size_t limit = MIN2(ZAddressOffsetMax, ZAddressSpaceLimit::heap());
  const size_t normal_size = max_capacity * ZVirtualToPhysicalRatio;
  const size_t extended_size = normal_size + max_capacity;
  const bool reserve_extra_space = ZNUMA::count() > 1 && extended_size <= limit;
  const size_t size = reserve_extra_space ? extended_size : MIN2(normal_size, limit);

  auto do_reserve = [&]() {
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
  };

  size_t reserved = do_reserve();

  if (reserve_extra_space) {
    // Did we attempt to reserve extra space

    if (reserved == extended_size) {
      // We successfully reserved the extra space

      _reserved_extra_space = true;
    } else if (reserved > normal_size) {
      // We reserved extra space, but not enough
      ZArray<ZVirtualMemory> to_unreserve;
      const size_t to_unreserve_size = reserved - normal_size;
      _extra_node.remove_from_high_many(to_unreserve_size, &to_unreserve);

      for (const ZVirtualMemory vmem : to_unreserve) {
        // Unreserve address
        const zaddress_unsafe addr = ZOffset::address_unsafe(vmem.start());

        // Reserve address space
        pd_unreserve(addr, vmem.size());
      }
    }
  }

  const bool is_contiguous = _extra_node.is_contiguous();

  log_info_p(gc, init)("Address Space Type: %s/%s/%s",
                       (is_contiguous ? "Contiguous" : "Discontiguous"),
                       (limit == ZAddressOffsetMax ? "Unrestricted" : "Restricted"),
                       (reserved >= normal_size ? "Complete" : "Degraded"));
  log_info_p(gc, init)("Address Space Size: %zuM", reserved / M);

  if (reserve_extra_space) {
    if (_reserved_extra_space) {
      log_debug_p(gc, init)("Reserving extra space for NUMA");
    } else {
      log_debug_p(gc, init)("Failed to reserve extra space for NUMA");
    }
  }

  return reserved;
}

bool ZVirtualMemoryManager::is_initialized() const {
  return _initialized;
}

bool ZVirtualMemoryManager::reserved_extra_space() const {
  return _reserved_extra_space;
}

ZVirtualMemory ZVirtualMemoryManager::remove_from_extra_space(size_t size) {
  return _extra_node.remove_from_low(size);
}

void ZVirtualMemoryManager::insert_extra_space(const ZVirtualMemory& vmem) {
  _extra_node.insert(vmem.start(), vmem.size());
}

bool ZVirtualMemoryManager::is_in_extra_space(const ZVirtualMemory& vmem) const {
  const ZVirtualMemory& range = _extra_space_range;
  return (!vmem.is_null() && vmem.start() >= range.start() && vmem.end() <= range.end());
}

void ZVirtualMemoryManager::insert_and_remove_from_low_many(const ZVirtualMemory& vmem, uint32_t numa_id, ZArray<ZVirtualMemory>* vmems_out) {
  _nodes.get(numa_id).insert_and_remove_from_low_many(vmem.start(), vmem.size(), vmems_out);
}

ZVirtualMemory ZVirtualMemoryManager::insert_and_remove_from_low_exact_or_many(size_t size, uint32_t numa_id, ZArray<ZVirtualMemory>* vmems_in_out) {
  return _nodes.get(numa_id).insert_and_remove_from_low_exact_or_many(size, vmems_in_out);
}

size_t ZVirtualMemoryManager::remove_from_low_many_at_most(size_t size, uint32_t numa_id, ZArray<ZVirtualMemory>* vmems_out) {
  return _nodes.get(numa_id).remove_from_low_many_at_most(size, vmems_out);
}

ZVirtualMemory ZVirtualMemoryManager::remove_low_address(size_t size, uint32_t numa_id) {
  return _nodes.get(numa_id).remove_from_low(size);
}

void ZVirtualMemoryManager::insert(const ZVirtualMemory& vmem) {
  const uint32_t numa_id = get_numa_id(vmem);
  _nodes.get(numa_id).insert(vmem.start(), vmem.size());
}

void ZVirtualMemoryManager::insert(const ZVirtualMemory& vmem, uint32_t numa_id) {
  assert(numa_id == get_numa_id(vmem), "wrong numa_id for vmem");
  _nodes.get(numa_id).insert(vmem.start(), vmem.size());
}

uint32_t ZVirtualMemoryManager::get_numa_id(const ZVirtualMemory& vmem) const {
  for (uint32_t numa_id = 0; numa_id < ZNUMA::count(); numa_id++) {
    const ZVirtualMemory& range = _vmem_ranges.get(numa_id);
    if (!vmem.is_null() && vmem.start() >= range.start() && vmem.end() <= range.end()) {
      return numa_id;
    }
  }

  assert(false, "Should never reach here");
  return -1;
}

zoffset ZVirtualMemoryManager::lowest_available_address(uint32_t numa_id) const {
  return _nodes.get(numa_id).peek_low_address();
}
