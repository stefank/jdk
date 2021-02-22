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

#ifndef SHARE_MEMORY_CHEAPALLOCATOR_HPP
#define SHARE_MEMORY_CHEAPALLOCATOR_HPP

#include "cppstdlib/type_traits.hpp"
#include "memory/allocation.hpp"
#include "utilities/debug.hpp"
#include "utilities/globalDefinitions.hpp"

// Base class for CHeapAllocator, providing access to the memflags.
// By default the template parameter flags are the memflags.
// The class is empty, so EBO applies.
template<MEMFLAGS flags>
class CHeapAllocatorFlags {
  static_assert(static_cast<int>(flags) < mt_number_of_types, "invalid memflags");
  static_assert(static_cast<int>(flags) >= 0, "negative memflags");
  static_assert(flags != mtNone, "Not for resource allocation");

public:
  constexpr explicit CHeapAllocatorFlags(MEMFLAGS) {}

  constexpr MEMFLAGS memflags() const { return flags; }
};

// Specialization for a dynamic memflags value provided as a constructor
// argument.  The memflags value is provided as a constructor argument that
// is recorded in a data member.
template<>
class CHeapAllocatorFlags<MEMFLAGS::dynamic_memory_type> {
  MEMFLAGS _memflags;

public:
  explicit CHeapAllocatorFlags(MEMFLAGS memflags) : _memflags(memflags) {
    assert(static_cast<int>(memflags) < mt_number_of_types, "invalid memflags");
    assert(static_cast<int>(memflags) >= 0, "negative memflags");
    assert(memflags != mtNone, "Not for resource allocation");
  }

  MEMFLAGS memflags() const { return _memflags; }
};

// Allocator that uses the HotSpot C Heap facilities.  The memory type may
// be static or dynamic.  It is static when the flags template parameter
// value is an explicit valid memory type.  It is dynamic when the flags
// template parameter is MEMFLAGS::dynamic_memory_type (the default), in
// which case the memory type must be provided as a constructor argument
// for the allocator.
template<typename T, MEMFLAGS flags = MEMFLAGS::dynamic_memory_type>
class CHeapAllocator : private CHeapAllocatorFlags<flags> {
  using Base = CHeapAllocatorFlags<flags>;
  static const bool is_dynamic = (flags == MEMFLAGS::dynamic_memory_type);

public:
  using value_type = T;
  using pointer = T*;
  using size_type = size_t;

  MEMFLAGS memflags() const { return Base::memflags(); }

  // Default constructor is only provided for the static case.
  // The dynamic case is not default constructible.
  template<typename Dummy = int, std::enable_if_t<!is_dynamic, Dummy> = 0>
  CHeapAllocator() noexcept : Base(flags) {}

  // Constructor with memflags argument is only provided for the dynamic case.
  // The static case does not provide this constructor.
  template<typename Dummy = int, std::enable_if_t<is_dynamic, Dummy> = 0>
  explicit CHeapAllocator(MEMFLAGS memflags) noexcept : Base(memflags) {}

  // Conversion constructor used when rebinding.
  template<typename U>
  CHeapAllocator(const CHeapAllocator<U, flags>& cha) noexcept
    : Base(cha.memflags()) {}

  pointer allocate(size_type n) noexcept {
    return NEW_C_HEAP_ARRAY(T, n, memflags());
  }

  void deallocate(pointer p, size_type n) noexcept {
    FREE_C_HEAP_ARRAY(T, p);
  }

  template<typename U>
  struct rebind { using other = CHeapAllocator<U, flags>; };

  // Allocators are equal if they can deallocate each other's memory.  Since
  // deallocation doesn't depend on the memflags, all allocators are equal.
  // For pocxxx, we want to propagate when dynamic, to propagate the memory
  // type.  For static case the operations can't have "different" (but ==)
  // allocators, but propagation is a nop.  So always propagate.
  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;
  using is_always_equal = std::true_type;

  template<typename T2, MEMFLAGS memflags2>
  bool operator==(const CHeapAllocator<T2, memflags2>&) const { return true; }

  template<typename T2, MEMFLAGS memflags2>
  bool operator!=(const CHeapAllocator<T2, memflags2>&) const { return false; }
};

#endif // SHARE_MEMORY_CHEAPALLOCATOR_HPP
