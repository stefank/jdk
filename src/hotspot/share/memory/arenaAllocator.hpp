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

#ifndef SHARE_MEMORY_ARENAALLOCATOR_HPP
#define SHARE_MEMORY_ARENAALLOCATOR_HPP

#include "cppstdlib/type_traits.hpp"
#include "memory/arena.hpp"
#include "utilities/globalDefinitions.hpp"

template<typename T>
class ArenaAllocator {
  Arena* _arena;

public:
  using value_type = T;
  using pointer = value_type*;
  using size_type = size_t;

  explicit ArenaAllocator(Arena* arena) : _arena(arena) {}

  template<typename U>
  ArenaAllocator(const ArenaAllocator<U>& a2) : _arena(a2.arena()) {}

  pointer allocate(size_type n) noexcept {
    return NEW_ARENA_ARRAY(_arena, T, n);
  }

  void deallocate(pointer p, size_type size) noexcept {
    FREE_ARENA_ARRAY(_arena, T, p, size);
  }

  // Destruction does nothing.  Objects may be simply dropped by resetting
  // the arena, so destruction must not have interestingly observable effects.
  template<typename U> void destroy(U* p) noexcept {}

  template<typename U>
  struct rebind { using other = ArenaAllocator<U>; };

  // For pocxxx, always propagate the allocator to update the arena.
  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;

  Arena* arena() const { return _arena; }

  // ArenaAllocators are equal if they are using the same Arena.
  template<typename T2>
  bool operator==(const ArenaAllocator<T2>& a2) const {
    return arena() == a2.arena();
  }

  template<typename T2>
  bool operator!=(const ArenaAllocator<T2>& a2) const {
    return arena() != a2.arena();
  }
};

#endif // SHARE_MEMORY_ARENAALLOCATOR_HPP
