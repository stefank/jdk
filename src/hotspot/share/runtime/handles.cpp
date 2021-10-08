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

#include "precompiled.hpp"
#include "memory/allocation.inline.hpp"
#include "oops/constantPool.hpp"
#include "oops/method.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/thread.inline.hpp"

#ifdef ASSERT
void Handle::verify_links() const {
  if (_obj == NULL) {
    assert(_next == NULL, "invariant");
    assert(_prev == NULL, "invariant");
    return;
  }

  assert(_prev != NULL, "invariant");
  assert(_prev->_next == this, "invariant");

  assert(_next != NULL, "invariant");
  assert(_next->_prev == this, "invariant");

  assert(HandleList::handle_list_for(this)->is_in(this), "invariant");
}
#endif

Handle::Handle(const Handle& other) :
    Handle(other._obj, NULL, NULL) {
  if (_obj != NULL) {
    HandleList::handle_list_for(this)->add(this);
  }
}

Handle::~Handle() {
  unlink();
}

Handle& Handle::operator=(const Handle& other) {
  if (this != &other) {
    if (other._obj == NULL) {
      // Just unlink
      unlink();
    } else {
      if (_obj == NULL) {
        // Not yet linked
        HandleList::handle_list_for(this)->add(this);
      }
    }

    _obj = other._obj;
  }

  return *this;
}

HandleList::HandleList() : _head(NULL, &_head, &_head) {
  assert(is_empty(), "must be empty");
}

HandleList::HandleList(HandleList&& other) :
    HandleList() {
  *this = std::move(other);
  verify_head();
}

HandleList& HandleList::operator=(HandleList&& other) {
  assert(is_empty(), "invariant");
  other.verify_head();

  if (!other.is_empty()) {
    // Move nodes
    Handle* next = other._head._next;
    Handle* prev = other._head._prev;

    _head._next = next;
    _head._prev = prev;

    next->_prev = &_head;
    prev->_next = &_head;

    other._head._next = &other._head;
    other._head._prev = &other._head;
    assert(other.is_empty(), "must be empty");

    verify_head();
  }

  return *this;
}

HandleList::~HandleList() {
  verify_head();
  // Prepare for destruction of _head
  assert(_head._obj == NULL, "should never be set");
  _head._next = NULL;
  _head._prev = NULL;
}

void HandleList::link(Handle* handle) {
  verify_head();
  handle->_prev = &_head;
  handle->_next = _head._next;

  _head._next = handle;

  handle->_next->_prev = handle;
  verify_head();
}

void HandleList::add(Handle* handle) {
  link(handle);
}

bool HandleList::is_in(const Handle* handle) const {
  for (const Handle* current = _head._next; current != &_head; current = current->_next) {
    if (current == handle) {
      return true;
    }
  }

  return false;
}

void HandleList::clear() {
  _head._next = &_head;
  _head._prev = &_head;
  assert(is_empty(), "must be empty");
  verify_head();
}

void HandleList::clear_handles() {
  verify_head();
#ifdef ASSERT
  for (Handle* current = _head._next; current != &_head; current = current->_next) {
    current->_obj = NULL;
  }
#endif
  clear();
}

void HandleList::oops_do(OopClosure* cl) const {
  verify_head();
  for (Handle* current = _head._next; current != &_head; current = current->_next) {
    cl->do_oop(&current->_obj);
  }
}

bool HandleList::is_empty() const {
  bool empty = _head._next == &_head;
  assert(empty == (_head._prev == &_head), "Both should match empty state");
  return empty;
}

HandleList* HandleList::handle_list_for(const Handle* handle) {
  Thread* thread = Thread::current();

  if (thread->is_in_live_stack((address)handle)) {
    return thread->handle_list();
  }

  HandleList* resource_handle_list = thread->resource_area()->handle_list_for(handle);
  if (resource_handle_list != NULL) {
    // Handle is allocated inside resource area,
    // return the list of the associated resource mark.
    return resource_handle_list;
  }

  fatal("Where is this allocated?");

  return thread->handle_list();
}

#ifdef ASSERT
void HandleList::verify_linked(const Handle* handle) const {
  assert(handle->_next->_prev == handle, "invariant");
  assert(handle->_prev->_next == handle, "invariant");
}

void HandleList::verify_head() const {
  verify_linked(&_head);
}
#endif

#ifdef ASSERT
oop* HandleArea::allocate_handle(oop obj) {
  assert(_handle_mark_nesting > 1, "memory leak: allocating handle outside HandleMark");
  assert(_no_handle_mark_nesting == 0, "allocating handle inside NoHandleMark");
  assert(oopDesc::is_oop(obj), "not an oop: " INTPTR_FORMAT, p2i(obj));
  return real_allocate_handle(obj);
}
#endif

// Copy constructors and destructors for metadata handles
// These do too much to inline.
#define DEF_METADATA_HANDLE_FN_NOINLINE(name, type) \
name##Handle::name##Handle(const name##Handle &h) {                    \
  _value = h._value;                                                   \
  if (_value != NULL) {                                                \
    assert(_value->is_valid(), "obj is valid");                        \
    if (h._thread != NULL) {                                           \
      assert(h._thread == Thread::current(), "thread must be current");\
      _thread = h._thread;                                             \
    } else {                                                           \
      _thread = Thread::current();                                     \
    }                                                                  \
    assert(_thread->is_in_live_stack((address)this), "not on stack?"); \
    _thread->metadata_handles()->push((Metadata*)_value);              \
  } else {                                                             \
    _thread = NULL;                                                    \
  }                                                                    \
}                                                                      \
name##Handle& name##Handle::operator=(const name##Handle &s) {         \
  remove();                                                            \
  _value = s._value;                                                   \
  if (_value != NULL) {                                                \
    assert(_value->is_valid(), "obj is valid");                        \
    if (s._thread != NULL) {                                           \
      assert(s._thread == Thread::current(), "thread must be current");\
      _thread = s._thread;                                             \
    } else {                                                           \
      _thread = Thread::current();                                     \
    }                                                                  \
    assert(_thread->is_in_live_stack((address)this), "not on stack?"); \
    _thread->metadata_handles()->push((Metadata*)_value);              \
  } else {                                                             \
    _thread = NULL;                                                    \
  }                                                                    \
  return *this;                                                        \
}                                                                      \
inline void name##Handle::remove() {                                   \
  if (_value != NULL) {                                                \
    int i = _thread->metadata_handles()->find_from_end((Metadata*)_value); \
    assert(i!=-1, "not in metadata_handles list");                     \
    _thread->metadata_handles()->remove_at(i);                         \
  }                                                                    \
}                                                                      \
name##Handle::~name##Handle () { remove(); }                           \

DEF_METADATA_HANDLE_FN_NOINLINE(method, Method)
DEF_METADATA_HANDLE_FN_NOINLINE(constantPool, ConstantPool)


static uintx chunk_oops_do(OopClosure* f, Chunk* chunk, char* chunk_top) {
  oop* bottom = (oop*) chunk->bottom();
  oop* top    = (oop*) chunk_top;
  uintx handles_visited = top - bottom;
  assert(top >= bottom && top <= (oop*) chunk->top(), "just checking");
  // during GC phase 3, a handle may be a forward pointer that
  // is not yet valid, so loosen the assertion
  while (bottom < top) {
    f->do_oop(bottom++);
  }
  return handles_visited;
}

void HandleArea::oops_do(OopClosure* f) {
  uintx handles_visited = 0;
  // First handle the current chunk. It is filled to the high water mark.
  handles_visited += chunk_oops_do(f, _chunk, _hwm);
  // Then handle all previous chunks. They are completely filled.
  Chunk* k = _first;
  while(k != _chunk) {
    handles_visited += chunk_oops_do(f, k, k->top());
    k = k->next();
  }

  if (_prev != NULL) _prev->oops_do(f);
}

void HandleMark::initialize(Thread* thread) {
  _thread = thread;  // Not the current thread during thread creation.
  // Save area
  _area  = thread->handle_area();
  // Save current top
  _chunk = _area->_chunk;
  _hwm   = _area->_hwm;
  _max   = _area->_max;
  _size_in_bytes = _area->_size_in_bytes;
  debug_only(_area->_handle_mark_nesting++);
  assert(_area->_handle_mark_nesting > 0, "must stack allocate HandleMarks");

  // Link this in the thread
  set_previous_handle_mark(thread->last_handle_mark());
  thread->set_last_handle_mark(this);
}

HandleMark::~HandleMark() {
  assert(_area == _thread->handle_area(), "sanity check");
  assert(_area->_handle_mark_nesting > 0, "must stack allocate HandleMarks" );

  pop_and_restore();
#ifdef ASSERT
  // clear out first chunk (to detect allocation bugs)
  if (ZapVMHandleArea) {
    memset(_hwm, badHandleValue, _max - _hwm);
  }
#endif

  // Unlink this from the thread
  _thread->set_last_handle_mark(previous_handle_mark());
}

void HandleMark::chop_later_chunks() {
  // reset arena size before delete chunks. Otherwise, the total
  // arena size could exceed total chunk size
  _area->set_size_in_bytes(size_in_bytes());
  _chunk->next_chop();
}

void* HandleMark::operator new(size_t size) throw() {
  return AllocateHeap(size, mtThread);
}

void* HandleMark::operator new [] (size_t size) throw() {
  return AllocateHeap(size, mtThread);
}

void HandleMark::operator delete(void* p) {
  FreeHeap(p);
}

void HandleMark::operator delete[](void* p) {
  FreeHeap(p);
}

#ifdef ASSERT

NoHandleMark::NoHandleMark() {
  HandleArea* area = Thread::current()->handle_area();
  area->_no_handle_mark_nesting++;
  assert(area->_no_handle_mark_nesting > 0, "must stack allocate NoHandleMark" );
}


NoHandleMark::~NoHandleMark() {
  HandleArea* area = Thread::current()->handle_area();
  assert(area->_no_handle_mark_nesting > 0, "must stack allocate NoHandleMark" );
  area->_no_handle_mark_nesting--;
}


ResetNoHandleMark::ResetNoHandleMark() {
  HandleArea* area = Thread::current()->handle_area();
  _no_handle_mark_nesting = area->_no_handle_mark_nesting;
  area->_no_handle_mark_nesting = 0;
}


ResetNoHandleMark::~ResetNoHandleMark() {
  HandleArea* area = Thread::current()->handle_area();
  area->_no_handle_mark_nesting = _no_handle_mark_nesting;
}

#endif // ASSERT
