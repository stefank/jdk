/*
 * Copyright (c) 2017, 2021, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_GC_Z_ORIGINAL_VMSTRUCTS_Z_ORIGINAL_HPP
#define SHARE_GC_Z_ORIGINAL_VMSTRUCTS_Z_ORIGINAL_HPP

#include "gc/z/original/zOriginalAttachedArray.hpp"
#include "gc/z/original/zOriginalCollectedHeap.hpp"
#include "gc/z/original/zOriginalForwarding.hpp"
#include "gc/z/original/zOriginalGranuleMap.hpp"
#include "gc/z/original/zOriginalHeap.hpp"
#include "gc/z/original/zOriginalPageAllocator.hpp"
#include "utilities/macros.hpp"

namespace ZOriginal {

// Expose some ZGC globals to the SA agent.
class ZGlobalsForVMStructs {
  static ZGlobalsForVMStructs _instance;

public:
  static ZGlobalsForVMStructs* _instance_p;

  ZGlobalsForVMStructs();

  uint32_t* _ZGlobalPhase;

  uint32_t* _ZGlobalSeqNum;

  uintptr_t* _ZAddressOffsetMask;
  uintptr_t* _ZAddressMetadataMask;
  uintptr_t* _ZAddressMetadataFinalizable;
  uintptr_t* _ZAddressGoodMask;
  uintptr_t* _ZAddressBadMask;
  uintptr_t* _ZAddressWeakBadMask;

  const int* _ZObjectAlignmentSmallShift;
  const int* _ZObjectAlignmentSmall;
};

typedef ZGranuleMap<ZPage*> ZGranuleMapForPageTable;
typedef ZGranuleMap<ZForwarding*> ZGranuleMapForForwarding;
typedef ZAttachedArray<ZForwarding, ZForwardingEntry> ZAttachedArrayForForwarding;

#define VM_STRUCTS_Z_ORIGINAL(nonstatic_field, volatile_nonstatic_field, static_field)                              \
  static_field(ZOriginal::ZGlobalsForVMStructs,            _instance_p,          ZOriginal::ZGlobalsForVMStructs*)  \
  nonstatic_field(ZOriginal::ZGlobalsForVMStructs,         _ZGlobalPhase,        uint32_t*)                         \
  nonstatic_field(ZOriginal::ZGlobalsForVMStructs,         _ZGlobalSeqNum,       uint32_t*)                         \
  nonstatic_field(ZOriginal::ZGlobalsForVMStructs,         _ZAddressOffsetMask,  uintptr_t*)                        \
  nonstatic_field(ZOriginal::ZGlobalsForVMStructs,         _ZAddressMetadataMask, uintptr_t*)                       \
  nonstatic_field(ZOriginal::ZGlobalsForVMStructs,         _ZAddressMetadataFinalizable, uintptr_t*)                \
  nonstatic_field(ZOriginal::ZGlobalsForVMStructs,         _ZAddressGoodMask,    uintptr_t*)                        \
  nonstatic_field(ZOriginal::ZGlobalsForVMStructs,         _ZAddressBadMask,     uintptr_t*)                        \
  nonstatic_field(ZOriginal::ZGlobalsForVMStructs,         _ZAddressWeakBadMask, uintptr_t*)                        \
  nonstatic_field(ZOriginal::ZGlobalsForVMStructs,         _ZObjectAlignmentSmallShift, const int*)                 \
  nonstatic_field(ZOriginal::ZGlobalsForVMStructs,         _ZObjectAlignmentSmall, const int*)                      \
                                                                                                                    \
  nonstatic_field(ZOriginal::ZCollectedHeap,               _heap,                ZOriginal::ZHeap)                  \
                                                                                                                    \
  nonstatic_field(ZOriginal::ZHeap,                        _page_allocator,      ZOriginal::ZPageAllocator)         \
  nonstatic_field(ZOriginal::ZHeap,                        _page_table,          ZOriginal::ZPageTable)             \
  nonstatic_field(ZOriginal::ZHeap,                        _forwarding_table,    ZOriginal::ZForwardingTable)       \
  nonstatic_field(ZOriginal::ZHeap,                        _relocate,            ZOriginal::ZRelocate)              \
                                                                                                                    \
  nonstatic_field(ZOriginal::ZPage,                        _type,                const uint8_t)                     \
  nonstatic_field(ZOriginal::ZPage,                        _seqnum,              uint32_t)                          \
  nonstatic_field(ZOriginal::ZPage,                        _virtual,             const ZOriginal::ZVirtualMemory)   \
  volatile_nonstatic_field(ZOriginal::ZPage,               _top,                 uintptr_t)                         \
                                                                                                                    \
  nonstatic_field(ZOriginal::ZPageAllocator,               _max_capacity,        const size_t)                      \
  volatile_nonstatic_field(ZOriginal::ZPageAllocator,      _capacity,            size_t)                            \
  volatile_nonstatic_field(ZOriginal::ZPageAllocator,      _used,                size_t)                            \
                                                                                                                    \
  nonstatic_field(ZOriginal::ZPageTable,                   _map,                 ZOriginal::ZGranuleMapForPageTable)\
                                                                                                                    \
  nonstatic_field(ZOriginal::ZGranuleMapForPageTable,      _map,                 ZOriginal::ZPage** const)          \
  nonstatic_field(ZOriginal::ZGranuleMapForForwarding,     _map,                 ZOriginal::ZForwarding** const)    \
                                                                                                                    \
  nonstatic_field(ZOriginal::ZForwardingTable,             _map,                 ZOriginal::ZGranuleMapForForwarding) \
                                                                                                                    \
  nonstatic_field(ZOriginal::ZVirtualMemory,               _start,               const uintptr_t)                   \
  nonstatic_field(ZOriginal::ZVirtualMemory,               _end,                 const uintptr_t)                   \
                                                                                                                    \
  nonstatic_field(ZOriginal::ZForwarding,                  _virtual,             const ZOriginal::ZVirtualMemory)   \
  nonstatic_field(ZOriginal::ZForwarding,                  _object_alignment_shift, const size_t)                   \
  volatile_nonstatic_field(ZOriginal::ZForwarding,         _ref_count,           int)                               \
  nonstatic_field(ZOriginal::ZForwarding,                  _entries,             const ZOriginal::ZAttachedArrayForForwarding) \
  nonstatic_field(ZOriginal::ZForwardingEntry,             _entry,               uint64_t)                          \
  nonstatic_field(ZOriginal::ZAttachedArrayForForwarding,  _length,              const size_t)

#define VM_INT_CONSTANTS_Z_ORIGINAL(declare_constant, declare_constant_with_value)                                  \
  declare_constant(ZOriginal::ZPhaseRelocate)                                                                       \
  declare_constant(ZOriginal::ZPageTypeSmall)                                                                       \
  declare_constant(ZOriginal::ZPageTypeMedium)                                                                      \
  declare_constant(ZOriginal::ZPageTypeLarge)                                                                       \
  declare_constant(ZOriginal::ZObjectAlignmentMediumShift)                                                          \
  declare_constant(ZOriginal::ZObjectAlignmentLargeShift)

#define VM_LONG_CONSTANTS_Z_ORIGINAL(declare_constant)                                                              \
  declare_constant(ZOriginal::ZGranuleSizeShift)                                                                    \
  declare_constant(ZOriginal::ZPageSizeSmallShift)                                                                  \
  declare_constant(ZOriginal::ZPageSizeMediumShift)                                                                 \
  declare_constant(ZOriginal::ZAddressOffsetShift)                                                                  \
  declare_constant(ZOriginal::ZAddressOffsetBits)                                                                   \
  declare_constant(ZOriginal::ZAddressOffsetMask)                                                                   \
  declare_constant(ZOriginal::ZAddressOffsetMax)

#define VM_TYPES_Z_ORIGINAL(declare_type, declare_toplevel_type, declare_integer_type)                              \
  declare_toplevel_type(ZOriginal::ZGlobalsForVMStructs)                                                            \
  declare_type(ZOriginal::ZCollectedHeap, CollectedHeap)                                                            \
  declare_toplevel_type(ZOriginal::ZHeap)                                                                           \
  declare_toplevel_type(ZOriginal::ZRelocate)                                                                       \
  declare_toplevel_type(ZOriginal::ZPage)                                                                           \
  declare_toplevel_type(ZOriginal::ZPageAllocator)                                                                  \
  declare_toplevel_type(ZOriginal::ZPageTable)                                                                      \
  declare_toplevel_type(ZOriginal::ZAttachedArrayForForwarding)                                                     \
  declare_toplevel_type(ZOriginal::ZGranuleMapForPageTable)                                                         \
  declare_toplevel_type(ZOriginal::ZGranuleMapForForwarding)                                                        \
  declare_toplevel_type(ZOriginal::ZVirtualMemory)                                                                  \
  declare_toplevel_type(ZOriginal::ZForwardingTable)                                                                \
  declare_toplevel_type(ZOriginal::ZForwarding)                                                                     \
  declare_toplevel_type(ZOriginal::ZForwardingEntry)                                                                \
  declare_toplevel_type(ZOriginal::ZPhysicalMemoryManager)

} // namespace ZOriginal

#endif // SHARE_GC_Z_ORIGINAL_VMSTRUCTS_Z_ORIGINAL_HPP
