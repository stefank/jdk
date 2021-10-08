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

#ifndef SHARE_MEMORY_RESOURCEAREA_HPP
#define SHARE_MEMORY_RESOURCEAREA_HPP

#include "memory/allocation.hpp"
#include "runtime/thread.hpp"

class ResourceMarkImpl;

// The resource area holds temporary data structures in the VM.
// The actual allocation areas are thread local. Typical usage:
//
//   ...
//   {
//     ResourceMark rm;
//     int foo[] = NEW_RESOURCE_ARRAY(int, 64);
//     ...
//   }
//   ...

//------------------------------ResourceArea-----------------------------------
// A ResourceArea is an Arena that supports safe usage of ResourceMark.
class ResourceArea: public Arena {
  friend class VMStructs;

  ResourceMarkImpl* _current_resource_mark;

#ifdef ASSERT
  int _nesting;                 // current # of nested ResourceMarks
  void verify_has_resource_mark();
#endif // ASSERT

public:
  ResourceArea(MEMFLAGS flags = mtThread) :
    Arena(flags), _current_resource_mark(NULL) DEBUG_ONLY(COMMA _nesting(0)) {}

  ResourceArea(size_t init_size, MEMFLAGS flags = mtThread) :
    Arena(flags, init_size), _current_resource_mark(NULL) DEBUG_ONLY(COMMA _nesting(0)) {}

  char* allocate_bytes(size_t size, AllocFailType alloc_failmode = AllocFailStrategy::EXIT_OOM);

  // Bias this resource area to specific memory type
  // (by default, ResourceArea is tagged as mtThread, per-thread general purpose storage)
  void bias_to(MEMFLAGS flags);

  DEBUG_ONLY(int nesting() const { return _nesting; })

  // Capture the state of a ResourceArea needed by a ResourceMark for
  // rollback to that mark.
  class SavedState {
    friend class ResourceArea;
    Chunk* _chunk;
    char* _hwm;
    char* _max;
    size_t _size_in_bytes;
    DEBUG_ONLY(int _nesting;)

  public:
    SavedState(const ResourceArea* area) :
      _chunk(area->_chunk),
      _hwm(area->_hwm),
      _max(area->_max),
      _size_in_bytes(area->_size_in_bytes)
      DEBUG_ONLY(COMMA _nesting(area->_nesting))
    {}

    static bool is_between(const void* mem, const SavedState* from, const SavedState* to) {
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
  };

  // Check and adjust debug-only nesting level.
  void activate_state(const SavedState& state) {
    assert(_nesting == state._nesting, "precondition");
    assert(_nesting >= 0, "precondition");
    assert(_nesting < INT_MAX, "nesting overflow");
    DEBUG_ONLY(++_nesting;)
  }

  // Check and adjust debug-only nesting level.
  void deactivate_state(const SavedState& state) {
    assert(_nesting > state._nesting, "deactivating inactive mark");
    assert((_nesting - state._nesting) == 1, "deactivating across another mark");
    DEBUG_ONLY(--_nesting;)
  }

  // Roll back the allocation state to the indicated state values.
  // The state must be the current state for this thread.
  void rollback_to(SavedState& state) {
    assert(_nesting > state._nesting, "rollback to inactive mark");
    assert((_nesting - state._nesting) == 1, "rollback across another mark");

    if (UseMallocOnly) {
      free_malloced_objects(state._chunk, state._hwm, state._max, _hwm);
    }

    if (state._chunk->next() != nullptr) { // Delete later chunks.
      // Reset size before deleting chunks.  Otherwise, the total
      // size could exceed the total chunk size.
      assert(size_in_bytes() > state._size_in_bytes,
             "size: " SIZE_FORMAT ", saved size: " SIZE_FORMAT,
             size_in_bytes(), state._size_in_bytes);
      set_size_in_bytes(state._size_in_bytes);
      state._chunk->next_chop();
    } else {
      assert(size_in_bytes() == state._size_in_bytes, "Sanity check");
    }
    _chunk = state._chunk;      // Roll back to saved chunk.
    _hwm = state._hwm;
    _max = state._max;

    // Clear out this chunk (to detect allocation bugs)
    if (ZapResourceArea) {
      memset(state._hwm, badResourceValue, state._max - state._hwm);
    }
  }

  ResourceMarkImpl* current_resource_mark();
  void set_current_resource_mark(ResourceMarkImpl* resource_mark);

  ResourceMarkImpl* resource_mark_for(const void* mem) const;
  HandleList* handle_list_for(const Handle* handle) const;

  // Visit all oops in Handles inside resource allocated objects.
  void oops_do(OopClosure* cl);
};


//------------------------------ResourceMark-----------------------------------
// A resource mark releases all resources allocated after it was constructed
// when the destructor is called.  Typically used as a local variable.

// Shared part of implementation for ResourceMark and DeoptResourceMark.
class ResourceMarkImpl {
  ResourceArea* _area;          // Resource area to stack allocate
  ResourceArea::SavedState _saved_state;

  Thread* _thread;
  ResourceMarkImpl* _previous_resource_mark;
  HandleList _handle_list;

  NONCOPYABLE(ResourceMarkImpl);

public:
  ResourceMarkImpl(Thread* thread, ResourceArea* area) :
    _area(area),
    _saved_state(area),
    _thread(thread),
    _previous_resource_mark(nullptr),
    _handle_list()
  {
    _area->activate_state(_saved_state);

    assert(thread != nullptr, "Show me where!");
    if (_thread != nullptr) {
      assert(_thread == Thread::current(), "not the current thread");
      _previous_resource_mark = area->current_resource_mark();
      area->set_current_resource_mark(this);
    }
  }

  explicit ResourceMarkImpl(Thread* thread)
    : ResourceMarkImpl(thread, thread->resource_area()) {}

  ~ResourceMarkImpl() {
    // Handles must be cleared before the call to reset_to_mark,
    // since it scribbles over the memory where the handles are allocated.
    _handle_list.clear_handles();
    assert(_thread != nullptr, "Show me where!");
    if (_thread != nullptr) {
      _area->set_current_resource_mark(_previous_resource_mark);
    }

    reset_to_mark();
    _area->deactivate_state(_saved_state);
  }

  void reset_to_mark() {
    _area->rollback_to(_saved_state);
  }

  ResourceMarkImpl* previous_resource_mark() {
    return _previous_resource_mark;
  }

  const ResourceArea::SavedState* saved_state() const {
    return &_saved_state;
  }

  HandleList* handle_list() {
    return &_handle_list;
  }

  void oops_do(OopClosure* cl);
};

class ResourceMark: public StackObj {
  ResourceMarkImpl _impl;

  NONCOPYABLE(ResourceMark);

  // Helper providing common constructor implementation.
  ResourceMark(Thread* thread, ResourceArea* area) :
    _impl(thread, area) {}

public:

  ResourceMark() : ResourceMark(Thread::current()) {}

  explicit ResourceMark(Thread* thread)
    : ResourceMark(thread, thread->resource_area()) {}

  explicit ResourceMark(ResourceArea* area)
    : ResourceMark(Thread::current_or_null(), area) {}

  void reset_to_mark() { _impl.reset_to_mark(); }

  const ResourceArea::SavedState* saved_state() const {
    return _impl.saved_state();
  }
};

//------------------------------DeoptResourceMark-----------------------------------
// A deopt resource mark releases all resources allocated after it was constructed
// when the destructor is called.  Typically used as a local variable. It differs
// from a typical resource more in that it is C-Heap allocated so that deoptimization
// can use data structures that are arena based but are not amenable to vanilla
// ResourceMarks because deoptimization can not use a stack allocated mark. During
// deoptimization we go thru the following steps:
//
// 0: start in assembly stub and call either uncommon_trap/fetch_unroll_info
// 1: create the vframeArray (contains pointers to Resource allocated structures)
//   This allocates the DeoptResourceMark.
// 2: return to assembly stub and remove stub frame and deoptee frame and create
//    the new skeletal frames.
// 3: push new stub frame and call unpack_frames
// 4: retrieve information from the vframeArray to populate the skeletal frames
// 5: release the DeoptResourceMark
// 6: return to stub and eventually to interpreter
//
// With old style eager deoptimization the vframeArray was created by the vmThread there
// was no way for the vframeArray to contain resource allocated objects and so
// a complex set of data structures to simulate an array of vframes in CHeap memory
// was used. With new style lazy deoptimization the vframeArray is created in the
// the thread that will use it and we can use a much simpler scheme for the vframeArray
// leveraging existing data structures if we simply create a way to manage this one
// special need for a ResourceMark. If ResourceMark simply inherited from CHeapObj
// then existing ResourceMarks would work fine since no one use new to allocate them
// and they would be stack allocated. This leaves open the possibility of accidental
// misuse so we duplicate the ResourceMark functionality via a shared implementation
// class.

class DeoptResourceMark: public CHeapObj<mtInternal> {
  ResourceMarkImpl _impl;

  NONCOPYABLE(DeoptResourceMark);

public:
  explicit DeoptResourceMark(Thread* thread) : _impl(thread) {}

  void reset_to_mark() { _impl.reset_to_mark(); }
};

#endif // SHARE_MEMORY_RESOURCEAREA_HPP
