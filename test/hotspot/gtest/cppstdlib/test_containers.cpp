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

#include "precompiled.hpp"

// Intentionally included out of order, before the containers. to verify
// that the assert fixup works.
#include "utilities/debug.hpp"

#include "cppstdlib/array.hpp"
#include "cppstdlib/deque.hpp"
#include "cppstdlib/forward_list.hpp"
#include "cppstdlib/functional.hpp"
#include "cppstdlib/list.hpp"
#include "cppstdlib/map.hpp"
#include "cppstdlib/queue.hpp"
#include "cppstdlib/set.hpp"
#include "cppstdlib/stack.hpp"
#include "cppstdlib/unordered_map.hpp"
#include "cppstdlib/unordered_set.hpp"
#include "cppstdlib/utility.hpp"
#include "memory/arenaAllocator.hpp"
#include "memory/cHeapAllocator.hpp"
#include "memory/resourceAreaAllocator.hpp"
#include "utilities/globalDefinitions.hpp"

#include "unittest.hpp"

// These tests are not intended to perform significant tests of the various
// standard container types.  They mostly verify we can include the associated
// headers, instantiate the types, and perform a few operations.  The point
// being to test the HotSpot wrapper headers are working.

TEST_VM(TestCppStdLibContainers, array) {
  std::array<uint, 5> c{ 0, 1, 2, 3, 4 };
  for (uint i = 0; i < c.size(); ++i) {
    ASSERT_EQ(c[i], i);
  }
}

// deque is one of the containers that uses allocators of multiple element
// types (via rebind), so uses the copy conversion between allocator types.
// Test that for the various allocator categories.
template<typename Alloc>
static void test_deque(Alloc allocator) {
  std::deque<uint, Alloc> c{allocator};
  for (uint i = 5; i < 10; ++i) { c.push_back(i); }
  for (uint i = 5; i > 0; ) { c.push_front(--i); }
  uint i = 0;
  for (uint j : c) { ASSERT_EQ(i++, j); }
}

TEST_VM(TestCppStdLibContainers, deque_cheap) {
  test_deque(CHeapAllocator<uint, mtInternal>());
}

TEST_VM(TestCppStdLibContainers, deque_resource) {
  ResourceMark rm;
  test_deque(ResourceAreaAllocator<uint>());
}

TEST_VM(TestCppStdLibContainers, deque_arena) {
  Arena arena{mtInternal};
  test_deque(ArenaAllocator<uint>(&arena));
}

template<typename C>
static void check_sequence(C& s) {
  for (uint i = 0; i < 10; ++i) { s.push_front(i); }
  uint i = 10;
  for (uint j : s) { ASSERT_EQ(--i, j); }
}

TEST_VM(TestCppStdLibContainers, forward_list) {
  std::forward_list<uint, CHeapAllocator<uint, mtInternal>> c;
  check_sequence(c);
}

TEST_VM(TestCppStdLibContainers, list) {
  std::list<uint, CHeapAllocator<uint, mtInternal>> c;
  check_sequence(c);
}

TEST_VM(TestCppStdLibContainers, map) {
  using Allocator = CHeapAllocator<std::pair<const uint, uint>, mtInternal>;
  std::map<uint, uint, std::less<uint>, Allocator> c;
  for (uint i = 0; i < 10; ++i) { c[i] = 10 - i; }
  uint i = 0;
  for (auto&& kv : c) {
    ASSERT_EQ(kv.first, i);
    ASSERT_EQ(kv.second, 10 - i);
    ++i;
  }
}

TEST_VM(TestCppStdLibContainers, set) {
  using Allocator = CHeapAllocator<uint, mtInternal>;
  std::set<uint, std::less<uint>, Allocator> c;
  for (uint i = 0; i < 10; ++i) { c.insert(10 - i); }
  uint i = 0;
  for (uint j : c) { ASSERT_EQ(++i, j); }
}

TEST_VM(TestCppStdLibContainers, unordered_map) {
  using Allocator = CHeapAllocator<std::pair<const uint, float>, mtInternal>;
  std::unordered_map<uint, float, std::hash<uint>, std::equal_to<uint>, Allocator> c;
  for (uint i = 0; i < 10; ++i) { c[i] = float(i); }
  for (uint i = 0; i < 10; ++i) { ASSERT_EQ(c[i], float(i)); }
}

TEST_VM(TestCppStdLibContainers, unordered_set) {
  using Allocator = CHeapAllocator<uint, mtInternal>;
  std::unordered_set<uint, std::hash<uint>, std::equal_to<uint>, Allocator> c;
  for (uint i = 0; i < 10; ++i) { c.insert(i); }
  for (uint i = 0; i < 10; ++i) { ASSERT_NE(c.find(i), c.end()); }
}

TEST_VM(TestCppStdLibContainers, queue) {
  using Container = std::deque<uint, CHeapAllocator<uint, mtInternal>>;
  Container c;
  std::queue<uint, Container> q(c);
  for (uint i = 0; i < 10; ++i) { q.push(i); }
  for (uint i = 0; i < 10; ++i, q.pop()) { ASSERT_EQ(q.front(), i); }
}

TEST_VM(TestCppStdLibContainers, stack) {
  using Container = std::deque<uint, CHeapAllocator<uint, mtInternal>>;
  Container c;
  std::stack<uint, Container> q(c);
  for (uint i = 0; i < 10; ++i) { q.push(9 - i); }
  for (uint i = 0; i < 10; ++i, q.pop()) { ASSERT_EQ(q.top(), i); }
}
