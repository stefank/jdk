/*
 * Copyright (c) 2019, 2025, Oracle and/or its affiliates. All rights reserved.
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

#include "gc/z/zAddress.inline.hpp"
#include "gc/z/zGlobals.hpp"
#include "gc/z/zLargePages.inline.hpp"
#include "gc/z/zMapper_windows.hpp"
#include "gc/z/zMemory.inline.hpp"
#include "gc/z/zSyscall_windows.hpp"
#include "gc/z/zValue.inline.hpp"
#include "gc/z/zVirtualMemoryManager.hpp"
#include "utilities/align.hpp"
#include "utilities/debug.hpp"

class ZVirtualMemoryReserverImpl : public CHeapObj<mtGC> {
public:
  virtual void register_callbacks(ZVirtualMemoryManager::ZMemoryManager* manager) {}
  virtual bool reserve(zaddress_unsafe addr, size_t size) = 0;
  virtual void unreserve(zaddress_unsafe addr, size_t size) = 0;
};

// Implements small pages (paged) support using placeholder reservation.
//
// When a memory range is available (kept by the virtual memory manager) a
// single placeholder is covering that memory range. When memory is
// removed from the manager the placeholder is split into granule
// sized placeholders to allow mapping operations on that granularity.
class ZVirtualMemoryReserverSmallPages : public ZVirtualMemoryReserverImpl {
private:
  class PlaceholderCallbacks : public AllStatic {
  private:
    static void split_placeholder(zoffset start, size_t size) {
      ZMapper::split_placeholder(ZOffset::address_unsafe(start), size);
    }

    static void coalesce_placeholders(zoffset start, size_t size) {
      ZMapper::coalesce_placeholders(ZOffset::address_unsafe(start), size);
    }

    // Turn the single placeholder covering the memory range into granule
    // sized placeholders.
    static void split_into_granule_sized_placeholders(zoffset start, size_t size) {
      assert(size >= ZGranuleSize, "Must be at least one granule");
      assert(is_aligned(size, ZGranuleSize), "Must be granule aligned");

      // Don't call split_placeholder on the last granule, since it is already
      // a placeholder and the system call would therefore fail.
      const size_t limit = size - ZGranuleSize;
      for (size_t offset = 0; offset < limit; offset += ZGranuleSize) {
        split_placeholder(start + offset, ZGranuleSize);
      }
    }

    static void coalesce_into_one_placeholder(zoffset start, size_t size) {
      assert(is_aligned(size, ZGranuleSize), "Must be granule aligned");

      // Granule sized ranges are already covered by a single placeholder
      if (size > ZGranuleSize) {
        coalesce_placeholders(start, size);
      }
    }

    // Callback implementations

    // Called when a memory range is returned to the memory manager but can't
    // be merged with an already existing range. Make sure this range is covered
    // by a single placeholder.
    static void insert_stand_alone_callback(const ZVirtualMemory& range) {
      assert(is_aligned(range.size(), ZGranuleSize), "Must be granule aligned");

      coalesce_into_one_placeholder(range.start(), range.size());
    }

    // Called when inserting a memory range and it can be merged at the start of an
    // existing range. Coalesce the underlying placeholders into one.
    static void insert_from_front_callback(const ZVirtualMemory& range, size_t size) {
      assert(is_aligned(range.size(), ZGranuleSize), "Must be granule aligned");

      const zoffset start = range.start() - size;
      coalesce_into_one_placeholder(start, range.size() + size);
    }

    // Called when inserting a memory range and it can be merged at the end of an
    // existing range. Coalesce the underlying placeholders into one.
    static void insert_from_back_callback(const ZVirtualMemory& range, size_t size) {
      assert(is_aligned(range.size(), ZGranuleSize), "Must be granule aligned");

      coalesce_into_one_placeholder(range.start(), range.size() + size);
    }

    // Called when a memory range is going to be handed out to be used.
    // This splits the memory range into granule sized placeholders.
    static void remove_stand_alone_callback(const ZVirtualMemory& range) {
      assert(is_aligned(range.size(), ZGranuleSize), "Must be granule aligned");

      split_into_granule_sized_placeholders(range.start(), range.size());
    }

    // Called when a memory range is removed at the front of an existing memory range.
    // Turn the first part of the memory range into granule sized placeholders.
    static void remove_from_front_callback(const ZVirtualMemory& range, size_t size) {
      assert(range.size() > size, "Must be larger than what we try to split out");
      assert(is_aligned(size, ZGranuleSize), "Must be granule aligned");

      // Split the range into two placeholders
      split_placeholder(range.start(), size);

      // Split the first part into granule sized placeholders
      split_into_granule_sized_placeholders(range.start(), size);
    }

    // Called when a memory range is removed at the end of an existing memory range.
    // Turn the second part of the memory range into granule sized placeholders.
    static void remove_from_back_callback(const ZVirtualMemory& range, size_t size) {
      assert(range.size() > size, "Must be larger than what we try to split out");
      assert(is_aligned(size, ZGranuleSize), "Must be granule aligned");

      // Split the range into two placeholders
      const zoffset start = to_zoffset(range.end() - size);
      split_placeholder(start, size);

      // Split the second part into granule sized placeholders
      split_into_granule_sized_placeholders(start, size);
    }

    // Called when transferring a memory range and it can be merged at the start of an
    // existing range. Coalesce the underlying placeholders into one.
    static void transfer_from_front_callback(const ZVirtualMemory& range, size_t size) {
      assert(range.size() > size, "Must be larger than what we try to split out");
      assert(is_aligned(range.size(), ZGranuleSize), "Must be granule aligned");

      // Split the range into two placeholders
      split_placeholder(range.start(), size);

      // Do not split the second part into granule sized placeholders.
      // The second part will be transfered over to another list.
    }

  public:
    static ZVirtualMemoryManager::ZMemoryManager::Callbacks callbacks() {
      // Each reserved virtual memory address range registered in _manager is
      // exactly covered by a single placeholder. Callbacks are installed so
      // that whenever a memory range changes, the corresponding placeholder
      // is adjusted.
      //
      // The insert and grow callbacks are called when virtual memory is
      // returned to the memory manager. The new memory range is then covered
      // by a new single placeholder.
      //
      // The remove and shrink callbacks are called when virtual memory is
      // removed from the memory manager. The memory range is then is split
      // into granule-sized placeholders.
      //
      // The transfer callback is called when virtual memory is transferred
      // from one memory manager to another. The resulting memory ranges are
      // are covered by two separate placeholders.
      //
      // See comment in zMapper_windows.cpp explaining why placeholders are
      // split into ZGranuleSize sized placeholders.

      ZVirtualMemoryManager::ZMemoryManager::Callbacks callbacks;

      callbacks._insert_stand_alone = &insert_stand_alone_callback;
      callbacks._insert_from_front = &insert_from_front_callback;
      callbacks._insert_from_back = &insert_from_back_callback;

      callbacks._remove_stand_alone = &remove_stand_alone_callback;
      callbacks._remove_from_front = &remove_from_front_callback;
      callbacks._remove_from_back = &remove_from_back_callback;

      callbacks._transfer_from_front = &transfer_from_front_callback;

      return callbacks;
    }
  };

  virtual void register_callbacks(ZVirtualMemoryManager::ZMemoryManager* manager) {
    manager->register_callbacks(PlaceholderCallbacks::callbacks());
  }

  virtual bool reserve(zaddress_unsafe addr, size_t size) {
    const zaddress_unsafe res = ZMapper::reserve(addr, size);

    assert(res == addr || untype(res) == 0, "Should not reserve other memory than requested");
    return res == addr;
  }

  virtual void unreserve(zaddress_unsafe addr, size_t size) {
    ZMapper::unreserve(addr, size);
  }
};

// Implements Large Pages (locked) support using shared AWE physical memory.

// ZPhysicalMemory layer needs access to the section
HANDLE ZAWESection;

class ZVirtualMemoryReserverLargePages : public ZVirtualMemoryReserverImpl {
private:
  virtual bool reserve(zaddress_unsafe addr, size_t size) {
    const zaddress_unsafe res = ZMapper::reserve_for_shared_awe(ZAWESection, addr, size);

    assert(res == addr || untype(res) == 0, "Should not reserve other memory than requested");
    return res == addr;
  }

  virtual void unreserve(zaddress_unsafe addr, size_t size) {
    ZMapper::unreserve_for_shared_awe(addr, size);
  }

public:
  ZVirtualMemoryReserverLargePages() {
    ZAWESection = ZMapper::create_shared_awe_section();
  }
};

static ZVirtualMemoryReserverImpl* _impl = nullptr;

void ZVirtualMemoryReserver::pd_initialize_before_reserve() {
  assert(_impl == nullptr, "Should only initialize once");

  if (ZLargePages::is_enabled()) {
    _impl = new ZVirtualMemoryReserverLargePages();
  } else {
    _impl = new ZVirtualMemoryReserverSmallPages();
  }
}

void ZVirtualMemoryReserver::pd_register_callbacks(ZVirtualMemoryManager::ZMemoryManager* manager) {
  _impl->register_callbacks(manager);
}

bool ZVirtualMemoryReserver::pd_reserve(zaddress_unsafe addr, size_t size) {
  return _impl->reserve(addr, size);
}

void ZVirtualMemoryReserver::pd_unreserve(zaddress_unsafe addr, size_t size) {
  _impl->unreserve(addr, size);
}
