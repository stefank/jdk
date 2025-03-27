/*
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_GC_Z_ZVIRTUALMEMORYMANAGER_INLINE_HPP
#define SHARE_GC_Z_ZVIRTUALMEMORYMANAGER_INLINE_HPP

#include "gc/z/zVirtualMemoryManager.hpp"

#include "gc/z/zMemory.inline.hpp"
#include "gc/z/zNUMA.inline.hpp"
#include "utilities/globalDefinitions.hpp"

inline bool ZVirtualMemoryManager::is_multi_node_enabled() const {
  return !_multi_node.is_empty();
}

inline bool ZVirtualMemoryManager::is_in_multi_node(const ZVirtualMemory& vmem) const {
  return _multi_node.limits_contain(vmem);
}

inline uint32_t ZVirtualMemoryManager::get_numa_id(const ZVirtualMemory& vmem) const {
  const uint32_t numa_nodes = ZNUMA::count();
  for (uint32_t numa_id = 0; numa_id < numa_nodes; numa_id++) {
    if (_nodes.get(numa_id).limits_contain(vmem)) {
      return numa_id;
    }
  }

  ShouldNotReachHere();
}

#endif // SHARE_GC_Z_ZVIRTUALMEMORYMANAGER_INLINE_HPP
