/*
 * Copyright (c) 1997, 2020, Oracle and/or its affiliates. All rights reserved.
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

#include "precompiled.hpp"
#include "memory/allocation.inline.hpp"
#include "memory/resourceArea.inline.hpp"
#include "prims/jvmtiUtil.hpp"
#include "runtime/atomic.hpp"
#include "runtime/thread.inline.hpp"
#include "services/memTracker.hpp"

void ResourceArea::bias_to(MEMFLAGS new_flags) {
  if (new_flags != _flags) {
    size_t size = size_in_bytes();
    MemTracker::record_arena_size_change(-ssize_t(size), _flags);
    MemTracker::record_arena_free(_flags);
    MemTracker::record_new_arena(new_flags);
    MemTracker::record_arena_size_change(ssize_t(size), new_flags);
    _flags = new_flags;
  }
}

ResourceMarkImpl* ResourceArea::current_resource_mark() {
  return _current_resource_mark;
}

void ResourceArea::set_current_resource_mark(ResourceMarkImpl* resource_mark) {
  _current_resource_mark = resource_mark;
}

ResourceMarkImpl* ResourceArea::resource_mark_for(const void* mem) const {
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

HandleList* ResourceArea::handle_list_for(const Handle* handle) const {
  ResourceMarkImpl* rm = resource_mark_for(handle);
  if (rm == NULL) {
    assert(!contains(handle), "Should have found a resource mark");
    return NULL;
  }

  return rm->handle_list();
}

void ResourceArea::oops_do(OopClosure* cl) {
  for (ResourceMarkImpl* current = _current_resource_mark; current != NULL; current = current->previous_resource_mark()) {
    current->oops_do(cl);
  }
}

#ifdef ASSERT

void ResourceArea::verify_has_resource_mark() {
  if (_nesting <= 0) {
    // Only report the first occurrence of an allocating thread that
    // is missing a ResourceMark, to avoid possible recursive errors
    // in error handling.
    static volatile bool reported = false;
    if (!Atomic::load(&reported)) {
      if (!Atomic::cmpxchg(&reported, false, true)) {
        fatal("memory leak: allocating without ResourceMark");
      }
    }
  }
}

#endif // ASSERT

//------------------------------ResourceMark-----------------------------------

ResourceMarkImpl::ResourceMarkImpl(Thread* thread, ResourceArea* area) :
  _area(area),
  _saved_state(area),
  _thread(thread),
  _previous_resource_mark(nullptr),
  _handle_list()
{
  _area->activate_state(_saved_state);

  if (_thread != nullptr) {
    assert(_thread == Thread::current(), "not the current thread");
  } else {
    assert(area == JvmtiUtil::single_threaded_resource_area(), "Show me where!");
  }

  // FIXME: area could be different from thread->resource_area().
  // When that happens, the GC will not know about the registered handles
  // in thie resource mark.

  _previous_resource_mark = area->current_resource_mark();
  area->set_current_resource_mark(this);
}

ResourceMarkImpl::ResourceMarkImpl(Thread* thread)
  : ResourceMarkImpl(thread, thread->resource_area()) {}

ResourceMarkImpl::~ResourceMarkImpl() {
  if (_thread != nullptr) {
    assert(_thread == Thread::current(), "not the current thread");
  } else {
    assert(_area == JvmtiUtil::single_threaded_resource_area(), "Show me where!");
  }

  _area->set_current_resource_mark(_previous_resource_mark);

  reset_to_mark();
  _area->deactivate_state(_saved_state);
}

void ResourceMarkImpl::oops_do(OopClosure* cl) {
  _handle_list.oops_do(cl);
}

// The following routines are declared in allocation.hpp and used everywhere:

// Allocation in thread-local resource area
extern char* resource_allocate_bytes(size_t size, AllocFailType alloc_failmode) {
  return Thread::current()->resource_area()->allocate_bytes(size, alloc_failmode);
}
extern char* resource_allocate_bytes(Thread* thread, size_t size, AllocFailType alloc_failmode) {
  return thread->resource_area()->allocate_bytes(size, alloc_failmode);
}

extern char* resource_reallocate_bytes( char *old, size_t old_size, size_t new_size, AllocFailType alloc_failmode){
  return (char*)Thread::current()->resource_area()->Arealloc(old, old_size, new_size, alloc_failmode);
}

extern void resource_free_bytes( char *old, size_t size ) {
  Thread::current()->resource_area()->Afree(old, size);
}
