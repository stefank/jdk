/*
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_MEMORY_RESOURCEAREAALLOCATOR_HPP
#define SHARE_MEMORY_RESOURCEAREAALLOCATOR_HPP

#include "cppstdlib/type_traits.hpp"
#include "memory/resourceArea.hpp"
#include "utilities/globalDefinitions.hpp"
#include "utilities/macros.hpp"

class Thread;

// Helper class for ResourceAreaAllocator.
class ResourceAreaAllocatorImpl {
  const ResourceMarkState* _rms;

  void check_allocator_state() const NOT_DEBUG_RETURN;

public:
  using size_type = size_t;

  ResourceAreaAllocatorImpl();
  // precondition: thread is current thread.
  explicit ResourceAreaAllocatorImpl(Thread* thread);
  ~ResourceAreaAllocatorImpl() = default;

#ifdef ASSERT
  ResourceAreaAllocatorImpl(const ResourceAreaAllocatorImpl& other);
  ResourceAreaAllocatorImpl& operator=(const ResourceAreaAllocatorImpl& other);
#endif // ASSERT

  // Allocate n bytes from the associated ResourceArea.
  void* allocate(size_type n) noexcept {
    check_allocator_state();
    return _rms->area()->allocate_bytes(n);
  }

  // Allocators are equal if they refer to the same ResourceMark.
  bool operator==(const ResourceAreaAllocatorImpl& other) const {
    return _rms == other._rms;
  }

  bool operator!=(const ResourceAreaAllocatorImpl& other) const {
    return _rms != other._rms;
  }
};

// Allocate from the thread-local resource area.
//
// A ResourceAreaAllocator is associated with the ResourceMark that
// was current when the allocator was constructed.  Two allocators
// are "compatible" if they are associated with the same ResourceMark.
//
// All allocations are relative to the associated ResourceMark, which is
// determined at the time the allocator is constructed.  Deallocation does
// nothing with the memory.  Instead, memory obtained from the allocator is
// implicitly reclaimed on exit from the associated ResourceMark.
//
// Construction must be within the context of a ResourceMark.
//
// Allocation from another thread is an error.
//
// Allocation from a ResourceMark nesting level different from that
// associated with the allocator is an error.
//
// Moving or assigning an allocator to an incompatible allocator is an
// error.
//
// Move-construct/assign at a ResourceMark nesting level different from that
// of the source container's allocator is an error.
//
// Swapping containers at a ResourceMark nesting level different from that
// for either container's allocator is an error.
template<typename T>
class ResourceAreaAllocator {
  ResourceAreaAllocatorImpl _impl;

  // For access to _impl of other specializations.
  template<typename U> friend class ResourceAreaAllocator;

public:
  using value_type = T;
  using pointer = T*;
  using size_type = ResourceAreaAllocatorImpl::size_type;

  ResourceAreaAllocator() : _impl() {}

  // precondition: thread is current thread.
  explicit ResourceAreaAllocator(Thread* thread) : _impl(thread) {}

  template<typename U>
  ResourceAreaAllocator(const ResourceAreaAllocator<U>& a2) : _impl(a2._impl) {}

  pointer allocate(size_type n) noexcept {
    return static_cast<pointer>(_impl.allocate(n * sizeof(T)));
  }

  // Deallocation just drops the memory.
  void deallocate(pointer p, size_type n) noexcept {}

  // Destruction does nothing.  Objects may be simply dropped by exiting the
  // associated resource mark, so destruction must not have interestingly
  // observable effects.
  template<typename U> void destroy(U* p) noexcept {}

  template<typename U>
  struct rebind { using other = ResourceAreaAllocator<U>; };

  // Allocators are equal iff they are compatible.
  //
  // In general, must not propagate when allocators are incompatible.  This
  // suggests pocxxx traits should all be false (default), and soccc should
  // construct a new allocator in the context in which it is called.  This
  // means that move-assign with incompatible allocators may need to
  // allocate space and must do per-element moves.  The performance impact
  // may be surprising.  It also means that swap with incompatible
  // allocators is (unchecked by us) UB.
  //
  // But what do we about move construction?  That will move (copy) the
  // allocator into the new container.  If the new container is in a
  // different context then it won't be able to use the allocator.  There is
  // nothing like soccc for move construction.
  //
  // So we document copying an allocator in a different context to be an
  // error, and we have an assertion against doing so.
  //
  // We can piggyback on that copy assertion to check for swap with
  // incompatible allocators by making pocs true.  That will effectively
  // assert compatible allocators when swapping container.
  //
  // We can similarly piggyback on that copy assertion to check for move
  // assign with incompatible allocators by making pocma true.  However,
  // this makes move-assign with incompatible allocators an error, rather
  // than being (perhaps unexpectedly) slower and allocating.
  //
  // It seems better to have container move-construct and move-assign have
  // similar behavior with regard to the allocator.  Hence we make pocma and
  // pocs true.

  // When copying a container, use allocator for the current context.
  ResourceAreaAllocator select_on_container_copy_construction() const {
    return ResourceAreaAllocator();
  }

  // Invoke allocator copy context check for container move-assign and swap.
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;

  template<typename T2>
  bool operator==(const ResourceAreaAllocator<T2>& other) const {
    return _impl == other._impl;
  }

  template<typename T2>
  bool operator!=(const ResourceAreaAllocator<T2>& other) const {
    return _impl != other._impl;
  }
};

#endif // SHARE_MEMORY_RESOURCEAREAALLOCATOR_HPP
