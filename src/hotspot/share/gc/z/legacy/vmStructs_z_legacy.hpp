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

#ifndef SHARE_GC_Z_LEGACY_VMSTRUCTS_Z_LEGACY_HPP
#define SHARE_GC_Z_LEGACY_VMSTRUCTS_Z_LEGACY_HPP

#include "gc/z/legacy/zLegacyAttachedArray.hpp"
#include "gc/z/legacy/zLegacyCollectedHeap.hpp"
#include "gc/z/legacy/zLegacyForwarding.hpp"
#include "gc/z/legacy/zLegacyGranuleMap.hpp"
#include "gc/z/legacy/zLegacyHeap.hpp"
#include "gc/z/legacy/zLegacyPageAllocator.hpp"
#include "utilities/macros.hpp"

namespace ZLegacy {

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

#define VM_STRUCTS_Z_LEGACY(nonstatic_field, volatile_nonstatic_field, static_field)                              \
  static_field(ZLegacy::ZGlobalsForVMStructs,            _instance_p,          ZLegacy::ZGlobalsForVMStructs*)    \
  nonstatic_field(ZLegacy::ZGlobalsForVMStructs,         _ZGlobalPhase,        uint32_t*)                         \
  nonstatic_field(ZLegacy::ZGlobalsForVMStructs,         _ZGlobalSeqNum,       uint32_t*)                         \
  nonstatic_field(ZLegacy::ZGlobalsForVMStructs,         _ZAddressOffsetMask,  uintptr_t*)                        \
  nonstatic_field(ZLegacy::ZGlobalsForVMStructs,         _ZAddressMetadataMask, uintptr_t*)                       \
  nonstatic_field(ZLegacy::ZGlobalsForVMStructs,         _ZAddressMetadataFinalizable, uintptr_t*)                \
  nonstatic_field(ZLegacy::ZGlobalsForVMStructs,         _ZAddressGoodMask,    uintptr_t*)                        \
  nonstatic_field(ZLegacy::ZGlobalsForVMStructs,         _ZAddressBadMask,     uintptr_t*)                        \
  nonstatic_field(ZLegacy::ZGlobalsForVMStructs,         _ZAddressWeakBadMask, uintptr_t*)                        \
  nonstatic_field(ZLegacy::ZGlobalsForVMStructs,         _ZObjectAlignmentSmallShift, const int*)                 \
  nonstatic_field(ZLegacy::ZGlobalsForVMStructs,         _ZObjectAlignmentSmall, const int*)                      \
                                                                                                                  \
  nonstatic_field(ZLegacy::ZCollectedHeap,               _heap,                ZLegacy::ZHeap)                    \
                                                                                                                  \
  nonstatic_field(ZLegacy::ZHeap,                        _page_allocator,      ZLegacy::ZPageAllocator)           \
  nonstatic_field(ZLegacy::ZHeap,                        _page_table,          ZLegacy::ZPageTable)               \
  nonstatic_field(ZLegacy::ZHeap,                        _forwarding_table,    ZLegacy::ZForwardingTable)         \
  nonstatic_field(ZLegacy::ZHeap,                        _relocate,            ZLegacy::ZRelocate)                \
                                                                                                                  \
  nonstatic_field(ZLegacy::ZPage,                        _type,                const uint8_t)                     \
  nonstatic_field(ZLegacy::ZPage,                        _seqnum,              uint32_t)                          \
  nonstatic_field(ZLegacy::ZPage,                        _virtual,             const ZLegacy::ZVirtualMemory)     \
  volatile_nonstatic_field(ZLegacy::ZPage,               _top,                 uintptr_t)                         \
                                                                                                                  \
  nonstatic_field(ZLegacy::ZPageAllocator,               _max_capacity,        const size_t)                      \
  volatile_nonstatic_field(ZLegacy::ZPageAllocator,      _capacity,            size_t)                            \
  volatile_nonstatic_field(ZLegacy::ZPageAllocator,      _used,                size_t)                            \
                                                                                                                  \
  nonstatic_field(ZLegacy::ZPageTable,                   _map,                 ZLegacy::ZGranuleMapForPageTable)  \
                                                                                                                  \
  nonstatic_field(ZLegacy::ZGranuleMapForPageTable,      _map,                 ZLegacy::ZPage** const)            \
  nonstatic_field(ZLegacy::ZGranuleMapForForwarding,     _map,                 ZLegacy::ZForwarding** const)      \
                                                                                                                  \
  nonstatic_field(ZLegacy::ZForwardingTable,             _map,                 ZLegacy::ZGranuleMapForForwarding) \
                                                                                                                  \
  nonstatic_field(ZLegacy::ZVirtualMemory,               _start,               const uintptr_t)                   \
  nonstatic_field(ZLegacy::ZVirtualMemory,               _end,                 const uintptr_t)                   \
                                                                                                                  \
  nonstatic_field(ZLegacy::ZForwarding,                  _virtual,             const ZLegacy::ZVirtualMemory)     \
  nonstatic_field(ZLegacy::ZForwarding,                  _object_alignment_shift, const size_t)                   \
  volatile_nonstatic_field(ZLegacy::ZForwarding,         _ref_count,           int)                               \
  nonstatic_field(ZLegacy::ZForwarding,                  _entries,             const ZLegacy::ZAttachedArrayForForwarding) \
  nonstatic_field(ZLegacy::ZForwardingEntry,             _entry,               uint64_t)                          \
  nonstatic_field(ZLegacy::ZAttachedArrayForForwarding,  _length,              const size_t)

#define VM_INT_CONSTANTS_Z_LEGACY(declare_constant, declare_constant_with_value)                                  \
  declare_constant(ZLegacy::ZPhaseRelocate)                                                                       \
  declare_constant(ZLegacy::ZPageTypeSmall)                                                                       \
  declare_constant(ZLegacy::ZPageTypeMedium)                                                                      \
  declare_constant(ZLegacy::ZPageTypeLarge)                                                                       \
  declare_constant(ZLegacy::ZObjectAlignmentMediumShift)                                                          \
  declare_constant(ZLegacy::ZObjectAlignmentLargeShift)

#define VM_LONG_CONSTANTS_Z_LEGACY(declare_constant)                                                              \
  declare_constant(ZLegacy::ZGranuleSizeShift)                                                                    \
  declare_constant(ZLegacy::ZPageSizeSmallShift)                                                                  \
  declare_constant(ZLegacy::ZPageSizeMediumShift)                                                                 \
  declare_constant(ZLegacy::ZAddressOffsetShift)                                                                  \
  declare_constant(ZLegacy::ZAddressOffsetBits)                                                                   \
  declare_constant(ZLegacy::ZAddressOffsetMask)                                                                   \
  declare_constant(ZLegacy::ZAddressOffsetMax)

#define VM_TYPES_Z_LEGACY(declare_type, declare_toplevel_type, declare_integer_type)                              \
  declare_toplevel_type(ZLegacy::ZGlobalsForVMStructs)                                                            \
  declare_type(ZLegacy::ZCollectedHeap, CollectedHeap)                                                            \
  declare_toplevel_type(ZLegacy::ZHeap)                                                                           \
  declare_toplevel_type(ZLegacy::ZRelocate)                                                                       \
  declare_toplevel_type(ZLegacy::ZPage)                                                                           \
  declare_toplevel_type(ZLegacy::ZPageAllocator)                                                                  \
  declare_toplevel_type(ZLegacy::ZPageTable)                                                                      \
  declare_toplevel_type(ZLegacy::ZAttachedArrayForForwarding)                                                     \
  declare_toplevel_type(ZLegacy::ZGranuleMapForPageTable)                                                         \
  declare_toplevel_type(ZLegacy::ZGranuleMapForForwarding)                                                        \
  declare_toplevel_type(ZLegacy::ZVirtualMemory)                                                                  \
  declare_toplevel_type(ZLegacy::ZForwardingTable)                                                                \
  declare_toplevel_type(ZLegacy::ZForwarding)                                                                     \
  declare_toplevel_type(ZLegacy::ZForwardingEntry)                                                                \
  declare_toplevel_type(ZLegacy::ZPhysicalMemoryManager)

} // namespace ZLegacy

#endif // SHARE_GC_Z_LEGACY_VMSTRUCTS_Z_LEGACY_HPP
