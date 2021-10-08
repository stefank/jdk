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

inline ResourceMarkImpl* ResourceArea::resource_mark_for(const void* mem) const {
  if (_current_resource_mark == NULL) {
    return NULL;
  }

  ResourceMarkImpl* rm = _current_resource_mark;

  SavedState initial(this);
  const SavedState* current = &initial;
  const SavedState* prev = rm->saved_state();

  while (true) {
    if (SavedState::is_between(mem, prev, current)) {
      return rm;
    }

    rm = rm->previous_resource_mark();
    if (rm == NULL) {
      // Done
      return NULL;
    }

    current = prev;
    prev = rm->saved_state();
  }
}

inline HandleList* ResourceArea::handle_list_for(const Handle* handle) const {
  ResourceMarkImpl* rm = resource_mark_for(handle);
  if (rm == NULL) {
    assert(!contains(handle), "Should have found a resource mark");
    return NULL;
  }

  return rm->handle_list();
}

#endif // SHARE_MEMORY_RESOURCEAREA_INLINE_HPP
