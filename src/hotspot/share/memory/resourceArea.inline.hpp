/*
 * Copyright (c) 1997, 2021, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_MEMORY_RESOURCEAREA_INLINE_HPP
#define SHARE_MEMORY_RESOURCEAREA_INLINE_HPP

#include "memory/resourceArea.hpp"

#include "services/memTracker.hpp"

inline char* ResourceArea::allocate_bytes(size_t size, AllocFailType alloc_failmode) {
#ifdef ASSERT
  verify_has_resource_mark();
  if (UseMallocOnly) {
    // use malloc, but save pointer in res. area for later freeing
    char** save = (char**)internal_amalloc(sizeof(char*));
    return (*save = (char*)os::malloc(size, mtThread, CURRENT_PC));
  }
#endif // ASSERT
  return (char*)Amalloc(size, alloc_failmode);
}

inline bool ResourceArea::SavedState::is_between(const void* mem, const SavedState* from, const SavedState* to) {
  if (from->_chunk == to->_chunk) {
    return mem >= from->_hwm && mem < to->_hwm;
  }

  // More than one chunk

  // Check first
  if (mem >= from->_hwm && mem < from->_max) {
    return true;
  }

  // Check in middle
  for (Chunk* chunk = from->_chunk; chunk != to->_chunk; chunk = chunk->next()) {
    // Filled chunks
    if (chunk->contains((char*)mem)) {
      return true;
    }
  }

  // Check last
  return mem >= to->_chunk->bottom() && mem < to->_chunk->top();
}

#endif // SHARE_MEMORY_RESOURCEAREA_INLINE_HPP
