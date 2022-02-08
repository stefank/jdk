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
#include "cppstdlib/memory.hpp"
#include "cppstdlib/utility.hpp"
#include "memory/allocation.hpp"
#include "memory/arena.hpp"
#include "memory/arenaAllocator.hpp"
#include "memory/resourceArea.hpp"
#include "utilities/cHeapVector.hpp"
#include "utilities/resourceAreaVector.hpp"
#include "unittest.hpp"

// Test Standard-style allocators, using std::vector.
// CHeapAllocator
// ResourceAreaAllocator
// ArenaAllocator

// volatile to defeat optimizations.
static volatile unsigned test_size = 1000;
static volatile unsigned test_iterations = 1000;

template<typename VectorType>
static void fill_test_vector(VectorType& v) {
  for (unsigned i = 0; i < test_size; ++i) {
    ASSERT_EQ(v.size(), i);
    v.push_back(i);
  }
  for (unsigned i = 0; i < test_size; ++i) {
    ASSERT_EQ(v[i], i);
  }
}

// Expect EBO with default allocator and static case of CHeapAllocator.
TEST_VM(StdAllocTestCHeapStatic, size) {
  ASSERT_EQ(sizeof(std::vector<unsigned>),
            sizeof(CHeapVector<unsigned, mtInternal>));
}

// Expect EBO with default allocator but not dynamic case of CHeapAllocator.
TEST_VM(StdAllocTestCHeapDynamic, size) {
  ASSERT_LT(sizeof(std::vector<unsigned>),
            sizeof(std::vector<unsigned, CHeapAllocator<unsigned>>));
}

TEST_VM(StdAllocTestCHeapStatic, stress_alloc) {
  for (unsigned i = 0; i < test_iterations; ++i) {
    CHeapVector<unsigned, mtInternal> v;
    fill_test_vector(v);
  }
}

TEST_VM(StdAllocTestCHeapDynamic, stress_alloc) {
  for (unsigned i = 0; i < test_iterations; ++i) {
    using Allocator = CHeapAllocator<unsigned>;
    Allocator a{mtInternal};
    std::vector<unsigned, Allocator> v{a};
    fill_test_vector(v);
  }
}

TEST_VM(StdAllocTestResourceArea, stress_alloc) {
  for (unsigned i = 0; i < test_iterations; ++i) {
    ResourceMark rm;
    ResourceAreaVector<unsigned> v;
    fill_test_vector(v);
  }
}

TEST_VM(StdAllocTestArena, stress_alloc) {
  using TestAllocator = ArenaAllocator<unsigned>;
  using TestVector = std::vector<unsigned, TestAllocator>;
  Arena arena(mtInternal);
  TestAllocator allocator(&arena);
  for (unsigned i = 0; i < test_iterations; ++i) {
    {
      TestVector v(allocator);
      fill_test_vector(v);
    }
    arena.destruct_contents();
  }
}

template<typename T>
static void fill_vector(T& v) {
  for (unsigned i = 0; i < 10; ++i) {
    v.push_back(i);
  }
}

// Abbreviations for allocator_traits used here.

template<typename Alloc>
static constexpr bool pocca(const Alloc&) {
  return std::allocator_traits<Alloc>::propagate_on_container_copy_assignment::value;
}

template<typename Alloc>
static constexpr bool pocma(const Alloc&) {
  return std::allocator_traits<Alloc>::propagate_on_container_move_assignment::value;
}

template<typename Alloc>
static constexpr bool pocs(const Alloc&) {
  return std::allocator_traits<Alloc>::propagate_on_container_swap::value;
}

template<typename Alloc>
static Alloc soccc(const Alloc& a) {
  return std::allocator_traits<Alloc>::select_on_container_copy_construction(a);
}

// Config classes are used to tailer tests to specific use-cases.
// A configured test is of the form
//
// (1) Establish an outer context, which provides an allocator.
// (2) Create an outer vector that uses the outer context's allocator.
// (3) Optionally collect some information about the outer vector or allocator.
// (4) Establish an inner context, which provides an allocator of the same
// type as the outer context.  The two allocators may be equal or not, depending
// on the configuration.
// (5) Create an inner vector, possibly by copying or moving from the outer vector.
// (6) Optionally perform some copyassign/moveassign/swap between the vectors.
// (7) Verify the resulting state.
//
// A Config class has the following requirements:
//
// (1) Allocator is a nested type that meets the Allocator requirements.
// (2) TestVector is a nested type, std::vector<unsigned, Allocator>.
// (3) OuterContext is a default constructible nested class, with member function
//   Allocator allocator()
// (4) InnerContext is a nested class with constructor
//   explicit InnerContext(OuterContext*)
// and member function
//   Allocator allocator()

class StdAllocTestCHeapStaticConfig {
public:
  using TestVector = CHeapVector<unsigned, mtInternal>;
  using Allocator = typename TestVector::allocator_type;

  struct OuterContext {
    Allocator allocator() { return Allocator(); }
  };

  struct InnerContext {
    explicit InnerContext(OuterContext*) {}
    Allocator allocator() { return Allocator(); }
  };
};

class StdAllocTestCHeapDynamicConfig {
public:
  using TestVector = std::vector<unsigned, CHeapAllocator<unsigned>>;
  using Allocator = typename TestVector::allocator_type;

  struct OuterContext {
    Allocator allocator() { return Allocator(mtInternal); }
  };

  struct InnerContext {
    explicit InnerContext(OuterContext*) {}
    Allocator allocator() { return Allocator(mtInternal); }
  };
};

// Config for testing operations on ResourceAreaVectors with the same allocator.
class StdAllocTestSameResourceAreaConfig {
public:
  using TestVector = ResourceAreaVector<unsigned>;
  using Allocator = typename TestVector::allocator_type;

  // OuterContext establishes a ResourceMark, and provides a
  // ResourceAreaAllocator associated with that mark.
  class OuterContext {
    ResourceMark _rm;
    Allocator _allocator;
  public:
    OuterContext() : _rm(), _allocator() {}
    Allocator allocator() { return _allocator; }
  };

  // InnerContext provides a ResourceAreaAllocator associated with the
  // current mark.  The outer context is ignored.
  class InnerContext {
    Allocator _allocator;
  public:
    explicit InnerContext(OuterContext* outer) : _allocator() {}
    Allocator allocator() { return _allocator; }
  };
};

// Config for testing operations on ResourceAreaVectors with different
// allocators.  Several tests die with this configuration, because using the
// outer context's allocator within the scope of the inner context isn't
// valid.  These tests are marked as death tests.
class StdAllocTestNestedResourceAreaConfig {
public:
  using TestVector = ResourceAreaVector<unsigned>;
  using Allocator = typename TestVector::allocator_type;

  // OuterContext establishes a ResourceMark, and provides a
  // ResourceAreaAllocator associated with that mark.
  class OuterContext {
    ResourceMark _rm;
    Allocator _allocator;
  public:
    OuterContext() : _rm(), _allocator() {}
    Allocator allocator() { return _allocator; }
  };

  // InnerContext establishes a ResourceMark, and provides a
  // ResourceAreaAllocator associated with that mark.  The outer context is
  // ignored.
  class InnerContext {
    ResourceMark _rm;
    Allocator _allocator;
  public:
    explicit InnerContext(OuterContext*) : _rm(), _allocator() {}
    Allocator allocator() { return _allocator; }
  };
};

// Config for testing operations on vectors with the same ArenaAllocator.
class StdAllocTestSameArenaConfig {
public:
  using Allocator = ArenaAllocator<unsigned>;
  using TestVector = std::vector<unsigned, Allocator>;

  // OuterContext creates an arena and provides an associated allocator.
  class OuterContext {
    Arena _arena;
    Allocator _allocator;
  public:
    OuterContext() : _arena(mtInternal), _allocator(&_arena) {}
    Allocator allocator() { return _allocator; }
  };

  // InnerContext provides the same allocator as the associated outer context.
  class InnerContext {
    Allocator _allocator;
  public:
    explicit InnerContext(OuterContext* outer) : _allocator(outer->allocator()) {}
    Allocator allocator() { return _allocator; }
  };
};

// Config for testing operations on vectors with different ArenaAllocators.
class StdAllocTestDifferentArenaConfig {
public:
  using Allocator = ArenaAllocator<unsigned>;
  using TestVector = std::vector<unsigned, Allocator>;

  // OuterContext creates an arena and provides an associated allocator.
  class OuterContext {
    Arena _arena;
    Allocator _allocator;
  public:
    OuterContext() : _arena(mtInternal), _allocator(&_arena) {}
    Allocator allocator() { return _allocator; }
  };

  // InnerContext creates an arena and provides an associated allocator.
  // The outer context is ignored.
  class InnerContext {
    Arena _arena;
    Allocator _allocator;
  public:
    explicit InnerContext(OuterContext*) : _arena(mtInternal), _allocator(&_arena) {}
    Allocator allocator() { return _allocator; }
  };
};

// Copy-construct inner vector from outer vector and inner allocator.
template<typename Config>
static void test_copy_explicit_allocator() {
  using TestVector = typename Config::TestVector;
  using OuterContext = typename Config::OuterContext;
  using InnerContext = typename Config::InnerContext;

  OuterContext outer_context;

  TestVector outer_vector{outer_context.allocator()};
  fill_vector(outer_vector);

  InnerContext inner_context{&outer_context};

  TestVector inner_vector{outer_vector, inner_context.allocator()};
  ASSERT_EQ(inner_vector.size(), outer_vector.size());
  ASSERT_NE(inner_vector.data(), outer_vector.data());
  ASSERT_EQ(inner_vector, outer_vector);
  ASSERT_EQ(inner_vector.get_allocator(), inner_context.allocator());
}

TEST_VM(StdAllocTestCHeapStatic, copy_explicit_allocator) {
  test_copy_explicit_allocator<StdAllocTestCHeapStaticConfig>();
}

TEST_VM(StdAllocTestCHeapDynamic, copy_explicit_allocator) {
  test_copy_explicit_allocator<StdAllocTestCHeapDynamicConfig>();
}

TEST_VM(StdAllocTestResourceArea, same_alloc_copy_explicit_allocator) {
  test_copy_explicit_allocator<StdAllocTestSameResourceAreaConfig>();
}

TEST_VM(StdAllocTestResourceArea, nested_alloc_copy_explicit_allocator) {
  test_copy_explicit_allocator<StdAllocTestNestedResourceAreaConfig>();
}

TEST_VM(StdAllocTestArena, same_alloc_copy_explicit_allocator) {
  test_copy_explicit_allocator<StdAllocTestSameArenaConfig>();
}

TEST_VM(StdAllocTestArena, different_alloc_copy_explicit_allocator) {
  test_copy_explicit_allocator<StdAllocTestDifferentArenaConfig>();
}

// Copy-construct inner vector from outer vector.  The inner vector's
// allocator is determined by SOCCC for the inner context's allocator.
template<typename Config>
static void test_copy_implicit_allocator() {
  using TestVector = typename Config::TestVector;
  using OuterContext = typename Config::OuterContext;
  using InnerContext = typename Config::InnerContext;
  using Allocator = typename Config::Allocator;

  OuterContext outer_context;
  // For nested resource alloc case, must get outer allocator outside scope
  // of inner context.
  Allocator outer_allocator = outer_context.allocator();

  TestVector outer_vector{outer_context.allocator()};
  fill_vector(outer_vector);

  InnerContext inner_context{&outer_context};

  TestVector inner_vector = outer_vector;
  ASSERT_EQ(inner_vector.size(), outer_vector.size());
  ASSERT_NE(inner_vector.data(), outer_vector.data());
  ASSERT_EQ(inner_vector, outer_vector);
  ASSERT_EQ(inner_vector.get_allocator(), soccc(outer_allocator));
}

TEST_VM(StdAllocTestCHeapStatic, copy_implicit_allocator) {
  test_copy_implicit_allocator<StdAllocTestCHeapStaticConfig>();
}

TEST_VM(StdAllocTestCHeapDynamic, copy_implicit_allocator) {
  test_copy_implicit_allocator<StdAllocTestCHeapDynamicConfig>();
}

TEST_VM(StdAllocTestResourceArea, same_alloc_copy_implicit_allocator) {
  test_copy_implicit_allocator<StdAllocTestSameResourceAreaConfig>();
}

TEST_VM(StdAllocTestResourceArea, nested_alloc_copy_implicit_allocator) {
  test_copy_implicit_allocator<StdAllocTestNestedResourceAreaConfig>();
}

TEST_VM(StdAllocTestArena, same_alloc_copy_implicit_allocator) {
  test_copy_implicit_allocator<StdAllocTestSameArenaConfig>();
}

TEST_VM(StdAllocTestArena, different_alloc_copy_implicit_allocator) {
  test_copy_implicit_allocator<StdAllocTestDifferentArenaConfig>();
}

// Move-construct inner vector from outer vector and inner allocator.
template<typename Config>
static void test_move_explicit_allocator() {
  using TestVector = typename Config::TestVector;
  using OuterContext = typename Config::OuterContext;
  using InnerContext = typename Config::InnerContext;

  OuterContext outer_context;

  TestVector outer_vector{outer_context.allocator()};
  fill_vector(outer_vector);
  size_t outer_size = outer_vector.size();
  typename TestVector::const_pointer outer_data = outer_vector.data();

  InnerContext inner_context{&outer_context};

  TestVector inner_vector{std::move(outer_vector), inner_context.allocator()};
  ASSERT_EQ(inner_vector.size(), outer_size);
  if (outer_context.allocator() == inner_context.allocator()) {
    ASSERT_EQ(inner_vector.data(), outer_data);
  } else {
    ASSERT_NE(inner_vector.data(), outer_data);
  }
  ASSERT_EQ(inner_vector.get_allocator(), inner_context.allocator());
}

TEST_VM(StdAllocTestCHeapStatic, move_explicit_allocator) {
  test_move_explicit_allocator<StdAllocTestCHeapStaticConfig>();
}

TEST_VM(StdAllocTestCHeapDynamic, move_explicit_allocator) {
  test_move_explicit_allocator<StdAllocTestCHeapDynamicConfig>();
}

TEST_VM(StdAllocTestResourceArea, same_alloc_move_explicit_allocator) {
  test_move_explicit_allocator<StdAllocTestSameResourceAreaConfig>();
}

#ifdef ASSERT
TEST_VM_ASSERT(StdAllocTestResourceArea, nested_alloc_move_explicit_allocator) {
  test_move_explicit_allocator<StdAllocTestNestedResourceAreaConfig>();
}
#endif // ASSERT

TEST_VM(StdAllocTestArena, same_alloc_move_explicit_allocator) {
  test_move_explicit_allocator<StdAllocTestSameArenaConfig>();
}

TEST_VM(StdAllocTestArena, different_alloc_move_explicit_allocator) {
  test_move_explicit_allocator<StdAllocTestDifferentArenaConfig>();
}

// Move-construct inner vector from outer vector.  The inner vector's
// allocator is also obtained from the outer vector.
template<typename Config>
static void test_move_implicit_allocator() {
  using TestVector = typename Config::TestVector;
  using OuterContext = typename Config::OuterContext;
  using InnerContext = typename Config::InnerContext;

  OuterContext outer_context;

  TestVector outer_vector{outer_context.allocator()};
  fill_vector(outer_vector);
  size_t outer_size = outer_vector.size();
  typename TestVector::const_pointer outer_data = outer_vector.data();

  InnerContext inner_context{&outer_context};

  TestVector inner_vector = std::move(outer_vector);
  ASSERT_EQ(inner_vector.size(), outer_size);
  ASSERT_EQ(inner_vector.data(), outer_data);
  ASSERT_EQ(inner_vector.get_allocator(), outer_context.allocator());
}

TEST_VM(StdAllocTestCHeapStatic, move_implicit_allocator) {
  test_move_implicit_allocator<StdAllocTestCHeapStaticConfig>();
}

TEST_VM(StdAllocTestCHeapDynamic, move_implicit_allocator) {
  test_move_implicit_allocator<StdAllocTestCHeapDynamicConfig>();
}

TEST_VM(StdAllocTestResourceArea, same_alloc_move_implicit_allocator) {
  test_move_implicit_allocator<StdAllocTestSameResourceAreaConfig>();
}

#ifdef ASSERT
TEST_VM_ASSERT(StdAllocTestResourceArea, nested_alloc_move_implicit_allocator) {
  test_move_implicit_allocator<StdAllocTestNestedResourceAreaConfig>();
}
#endif // ASSERT

TEST_VM(StdAllocTestArena, same_alloc_move_implicit_allocator) {
  test_move_implicit_allocator<StdAllocTestSameArenaConfig>();
}

TEST_VM(StdAllocTestArena, different_alloc_move_implicit_allocator) {
  test_move_implicit_allocator<StdAllocTestDifferentArenaConfig>();
}

// Copy-assign inner vector from outer vector.
template<typename Config>
static void test_copyassign_down() {
  using TestVector = typename Config::TestVector;
  using OuterContext = typename Config::OuterContext;
  using InnerContext = typename Config::InnerContext;

  OuterContext outer_context;

  TestVector outer_vector{outer_context.allocator()};
  fill_vector(outer_vector);
  fill_vector(outer_vector);
  size_t outer_size = outer_vector.size();
  typename TestVector::const_pointer outer_data = outer_vector.data();

  InnerContext inner_context{&outer_context};

  TestVector inner_vector{inner_context.allocator()};
  fill_vector(inner_vector);
  size_t inner_size = inner_vector.size();
  ASSERT_NE(inner_size, outer_size);

  inner_vector = outer_vector;

  // inner_vector matches outer_vector, but doesn't share data.
  ASSERT_EQ(inner_vector, outer_vector);
  ASSERT_NE(inner_vector.data(), outer_vector.data());
  if (pocca(inner_context.allocator())) {
    ASSERT_EQ(inner_vector.get_allocator(), outer_context.allocator());
  } else {
    ASSERT_EQ(inner_vector.get_allocator(), inner_context.allocator());
  }
}

TEST_VM(StdAllocTestCHeapStatic, copyassign_down) {
  test_copyassign_down<StdAllocTestCHeapStaticConfig>();
}

TEST_VM(StdAllocTestCHeapDynamic, copyassign_down) {
  test_copyassign_down<StdAllocTestCHeapDynamicConfig>();
}

TEST_VM(StdAllocTestResourceArea, same_alloc_copyassign_down) {
  test_copyassign_down<StdAllocTestSameResourceAreaConfig>();
}

TEST_VM(StdAllocTestResourceArea, nested_alloc_copyassign_down) {
  test_copyassign_down<StdAllocTestNestedResourceAreaConfig>();
}

TEST_VM(StdAllocTestArena, same_alloc_copyassign_down) {
  test_copyassign_down<StdAllocTestSameArenaConfig>();
}

TEST_VM(StdAllocTestArena, different_alloc_copyassign_down) {
  test_copyassign_down<StdAllocTestDifferentArenaConfig>();
}

// Copy-assign outer vector from inner vector.
template<typename Config>
static void test_copyassign_up() {
  using TestVector = typename Config::TestVector;
  using OuterContext = typename Config::OuterContext;
  using InnerContext = typename Config::InnerContext;

  OuterContext outer_context;

  TestVector outer_vector{outer_context.allocator()};
  fill_vector(outer_vector);

  InnerContext inner_context{&outer_context};

  TestVector inner_vector{inner_context.allocator()};
  // Inner is large enough to force outer to grow during copy.
  fill_vector(inner_vector);
  fill_vector(inner_vector);
  fill_vector(inner_vector);
  size_t inner_size = inner_vector.size();
  typename TestVector::const_pointer inner_data = inner_vector.data();
  ASSERT_NE(inner_size, outer_vector.size());

  outer_vector = inner_vector;

  // outer_vector matches inner_vector, but doesn't share data.
  ASSERT_EQ(outer_vector, inner_vector);
  ASSERT_NE(outer_vector.data(), inner_vector.data());
  if (pocca(outer_context.allocator())) {
    ASSERT_EQ(outer_vector.get_allocator(), inner_context.allocator());
  } else {
    ASSERT_EQ(outer_vector.get_allocator(), outer_context.allocator());
  }
}

TEST_VM(StdAllocTestCHeapStatic, copyassign_up) {
  test_copyassign_up<StdAllocTestCHeapStaticConfig>();
}

TEST_VM(StdAllocTestCHeapDynamic, copyassign_up) {
  test_copyassign_up<StdAllocTestCHeapDynamicConfig>();
}

TEST_VM(StdAllocTestResourceArea, same_alloc_copyassign_up) {
  test_copyassign_up<StdAllocTestSameResourceAreaConfig>();
}

#ifdef ASSERT
TEST_VM_ASSERT(StdAllocTestResourceArea, nested_alloc_copyassign_up) {
  test_copyassign_up<StdAllocTestNestedResourceAreaConfig>();
}
#endif // ASSERT

TEST_VM(StdAllocTestArena, same_alloc_copyassign_up) {
  test_copyassign_up<StdAllocTestSameArenaConfig>();
}

TEST_VM(StdAllocTestArena, different_alloc_copyassign_up) {
  test_copyassign_up<StdAllocTestDifferentArenaConfig>();
}

// Move-assign inner vector from outer vector.
template<typename Config>
static void test_moveassign_down() {
  using TestVector = typename Config::TestVector;
  using OuterContext = typename Config::OuterContext;
  using InnerContext = typename Config::InnerContext;

  OuterContext outer_context;

  TestVector outer_vector{outer_context.allocator()};
  fill_vector(outer_vector);
  fill_vector(outer_vector);
  size_t outer_size = outer_vector.size();
  typename TestVector::const_pointer outer_data = outer_vector.data();

  InnerContext inner_context{&outer_context};

  TestVector inner_vector{inner_context.allocator()};
  fill_vector(inner_vector);
  size_t inner_size = inner_vector.size();
  ASSERT_NE(inner_size, outer_size);

  inner_vector = std::move(outer_vector);

  // information transferred to inner.
  ASSERT_EQ(inner_vector.size(), outer_size);
  if ((inner_context.allocator() == outer_context.allocator()) ||
      pocma(inner_context.allocator())) {
    ASSERT_EQ(inner_vector.data(), outer_data);
    ASSERT_EQ(inner_vector.get_allocator(), outer_context.allocator());
  } else {
    ASSERT_NE(inner_vector.data(), outer_data);
    ASSERT_EQ(inner_vector.get_allocator(), inner_context.allocator());
  }
}

TEST_VM(StdAllocTestCHeapStatic, moveassign_down) {
  test_moveassign_down<StdAllocTestCHeapStaticConfig>();
}

TEST_VM(StdAllocTestCHeapDynamic, moveassign_down) {
  test_moveassign_down<StdAllocTestCHeapDynamicConfig>();
}

TEST_VM(StdAllocTestResourceArea, same_alloc_moveassign_down) {
  test_moveassign_down<StdAllocTestSameResourceAreaConfig>();
}

#ifdef ASSERT
TEST_VM_ASSERT(StdAllocTestResourceArea, nested_alloc_moveassign_down) {
  test_moveassign_down<StdAllocTestNestedResourceAreaConfig>();
}
#endif // ASSERT

TEST_VM(StdAllocTestArena, same_alloc_moveassign_down) {
  test_moveassign_down<StdAllocTestSameArenaConfig>();
}

TEST_VM(StdAllocTestArena, different_alloc_moveassign_down) {
  test_moveassign_down<StdAllocTestDifferentArenaConfig>();
}

// Move-assign outer vector from inner vector.
template<typename Config>
static void test_moveassign_up() {
  using TestVector = typename Config::TestVector;
  using OuterContext = typename Config::OuterContext;
  using InnerContext = typename Config::InnerContext;

  OuterContext outer_context;

  TestVector outer_vector{outer_context.allocator()};
  fill_vector(outer_vector);
  fill_vector(outer_vector);

  InnerContext inner_context{&outer_context};

  TestVector inner_vector{inner_context.allocator()};
  fill_vector(inner_vector);
  size_t inner_size = inner_vector.size();
  typename TestVector::const_pointer inner_data = inner_vector.data();
  ASSERT_NE(inner_size, outer_vector.size());

  outer_vector = std::move(inner_vector);

  // information transferred to outer.
  ASSERT_EQ(outer_vector.size(), inner_size);
  if ((inner_context.allocator() == outer_context.allocator()) ||
      pocma(outer_context.allocator())) {
    ASSERT_EQ(outer_vector.data(), inner_data);
    ASSERT_EQ(outer_vector.get_allocator(), inner_context.allocator());
  } else {
    ASSERT_NE(outer_vector.data(), inner_data);
    ASSERT_EQ(outer_vector.get_allocator(), outer_context.allocator());
  }
}

TEST_VM(StdAllocTestCHeapStatic, moveassign_up) {
  test_moveassign_up<StdAllocTestCHeapStaticConfig>();
}

TEST_VM(StdAllocTestCHeapDynamic, moveassign_up) {
  test_moveassign_up<StdAllocTestCHeapDynamicConfig>();
}

TEST_VM(StdAllocTestResourceArea, same_alloc_moveassign_up) {
  test_moveassign_up<StdAllocTestSameResourceAreaConfig>();
}

#ifdef ASSERT
TEST_VM_ASSERT(StdAllocTestResourceArea, nested_alloc_moveassign_up) {
  test_moveassign_up<StdAllocTestNestedResourceAreaConfig>();
}
#endif // ASSERT

TEST_VM(StdAllocTestArena, same_alloc_moveassign_up) {
  test_moveassign_up<StdAllocTestSameArenaConfig>();
}

TEST_VM(StdAllocTestArena, different_alloc_moveassign_up) {
  test_moveassign_up<StdAllocTestDifferentArenaConfig>();
}

// Swap vectors.
template<typename Config>
static void test_swap() {
  using TestVector = typename Config::TestVector;
  using OuterContext = typename Config::OuterContext;
  using InnerContext = typename Config::InnerContext;

  OuterContext outer_context;

  TestVector outer_vector{outer_context.allocator()};
  fill_vector(outer_vector);
  fill_vector(outer_vector);
  size_t outer_size = outer_vector.size();
  typename TestVector::const_pointer outer_data = outer_vector.data();

  InnerContext inner_context{&outer_context};

  TestVector inner_vector{inner_context.allocator()};
  fill_vector(inner_vector);
  size_t inner_size = inner_vector.size();
  typename TestVector::const_pointer inner_data = inner_vector.data();

  ASSERT_NE(inner_size, outer_size);
  ASSERT_NE(inner_data, outer_data);

  std::swap(inner_vector, outer_vector);

  ASSERT_EQ(outer_vector.size(), inner_size);
  ASSERT_EQ(outer_vector.data(), inner_data);

  ASSERT_EQ(inner_vector.size(), outer_size);
  ASSERT_EQ(inner_vector.data(), outer_data);

  if (outer_context.allocator() == inner_context.allocator()) {
    ASSERT_EQ(outer_vector.get_allocator(), outer_context.allocator());
    ASSERT_EQ(inner_vector.get_allocator(), inner_context.allocator());
  } else {
    ASSERT_TRUE(pocs(outer_context.allocator()));
    ASSERT_TRUE(pocs(inner_context.allocator()));
    ASSERT_EQ(outer_vector.get_allocator(), inner_context.allocator());
    ASSERT_EQ(inner_vector.get_allocator(), outer_context.allocator());
  }
}

TEST_VM(StdAllocTestCHeapStatic, swap) {
  test_swap<StdAllocTestCHeapStaticConfig>();
}

TEST_VM(StdAllocTestCHeapDynamic, swap) {
  test_swap<StdAllocTestCHeapDynamicConfig>();
}

TEST_VM(StdAllocTestResourceArea, same_alloc_swap) {
  test_swap<StdAllocTestSameResourceAreaConfig>();
}

#ifdef ASSERT
TEST_VM_ASSERT(StdAllocTestResourceArea, nested_alloc_swap) {
  test_swap<StdAllocTestNestedResourceAreaConfig>();
}
#endif // ASSERT

TEST_VM(StdAllocTestArena, same_alloc_swap) {
  test_swap<StdAllocTestSameArenaConfig>();
}

TEST_VM(StdAllocTestArena, different_alloc_swap) {
  test_swap<StdAllocTestDifferentArenaConfig>();
}

// Config classes for allocator propagation tests.
//
// A Config class has the following requirements:
//
// (1) Allocator is a nested type that meets the Allocator requirements.
// (2) TestVector is a nested type, std::vector<unsigned, Allocator>.
// (3) OuterContext and InnerContext are default constructible nested classes,
//   each with member function Allocator allocator().
// (4) <unspecified> value(Allocator) is a static member function returning
//   a value used to compare allocators.
//
// We don't need ResourceAreaAllocator tests here.  No propagation is
// permitted for them, and the normal allocator tests above cover the
// needed cases.

class StdAllocTestCHeapDynamicPropagationConfig {
public:
  using Allocator = CHeapAllocator<unsigned>;
  using TestVector = std::vector<unsigned, Allocator>;

  struct OuterContext {
    Allocator allocator() { return Allocator(mtInternal); }
  };

  struct InnerContext {
    Allocator allocator() { return Allocator(mtGC); }
  };

  static MEMFLAGS value(Allocator a) { return a.memflags(); }
};

class StdAllocTestArenaPropagationConfig {
public:
  using Allocator = ArenaAllocator<unsigned>;
  using TestVector = std::vector<unsigned, Allocator>;

  struct OuterContext {
    Arena _arena;
    OuterContext() : _arena(mtInternal) {}
    Allocator allocator() { return Allocator(&_arena); }
  };

  using InnerContext = OuterContext;

  static Arena* value(Allocator a) { return a.arena(); }
};

template<typename Config>
static void test_pocmc() {
  using TestVector = typename Config::TestVector;
  using OuterContext = typename Config::OuterContext;
  using InnerContext = typename Config::InnerContext;

  OuterContext outer_context;

  TestVector v1{outer_context.allocator()};
  fill_test_vector(v1);
  unsigned* v1_data = v1.data();
  size_t v1_size = v1.size();
  auto v1_value = Config::value(v1.get_allocator());

  TestVector v2{std::move(v1)};
  ASSERT_EQ(v2.data(), v1_data);
  ASSERT_EQ(v2.size(), v1_size);
  ASSERT_EQ(Config::value(v1.get_allocator()), v1_value);
}

TEST_VM(StdAllocTestCHeapDynamic, pocmc) {
  test_pocmc<StdAllocTestCHeapDynamicPropagationConfig>();
}

TEST_VM(StdAllocTestArena, pocmc) {
  test_pocmc<StdAllocTestArenaPropagationConfig>();
}

template<typename Config>
static void test_pocca() {
  using TestVector = typename Config::TestVector;
  using OuterContext = typename Config::OuterContext;
  using InnerContext = typename Config::InnerContext;

  OuterContext outer_context;

  TestVector v1{outer_context.allocator()};
  fill_test_vector(v1);

  InnerContext inner_context;

  TestVector v2{inner_context.allocator()};
  ASSERT_NE(Config::value(v1.get_allocator()), Config::value(v2.get_allocator()));

  v2 = v1;
  ASSERT_EQ(v1, v2);
  ASSERT_NE(v1.data(), v2.data());
  ASSERT_EQ(v1.size(), v2.size());
  ASSERT_EQ(Config::value(v1.get_allocator()), Config::value(v2.get_allocator()));
}

TEST_VM(StdAllocTestCHeapDynamic, pocca) {
  test_pocca<StdAllocTestCHeapDynamicPropagationConfig>();
}

TEST_VM(StdAllocTestArena, pocca) {
  test_pocca<StdAllocTestArenaPropagationConfig>();
}

template<typename Config>
static void test_pocma() {
  using TestVector = typename Config::TestVector;
  using Allocator = typename Config::Allocator;
  using OuterContext = typename Config::OuterContext;
  using InnerContext = typename Config::InnerContext;

  OuterContext outer_context;

  TestVector v1{outer_context.allocator()};
  fill_test_vector(v1);
  unsigned* v1_data = v1.data();
  size_t v1_size = v1.size();
  auto v1_value = Config::value(v1.get_allocator());

  InnerContext inner_context;

  TestVector v2{inner_context.allocator()};
  auto v2_value = Config::value(v2.get_allocator());
  ASSERT_NE(v1_value, v2_value);

  v2 = std::move(v1);
  ASSERT_EQ(v2.data(), v1_data);
  ASSERT_EQ(v2.size(), v1_size);
  ASSERT_EQ(Config::value(v2.get_allocator()), v1_value);
}

TEST_VM(StdAllocTestCHeapDynamic, pocma) {
  test_pocma<StdAllocTestCHeapDynamicPropagationConfig>();
}

TEST_VM(StdAllocTestArena, pocma) {
  test_pocma<StdAllocTestArenaPropagationConfig>();
}

template<typename Config>
static void test_pocs() {
  using TestVector = typename Config::TestVector;
  using Allocator = typename Config::Allocator;
  using OuterContext = typename Config::OuterContext;
  using InnerContext = typename Config::InnerContext;

  OuterContext outer_context;

  TestVector v1{outer_context.allocator()};
  fill_test_vector(v1);
  unsigned* v1_data = v1.data();
  size_t v1_size = v1.size();
  auto v1_value = Config::value(v1.get_allocator());

  InnerContext inner_context;

  TestVector v2{inner_context.allocator()};
  unsigned* v2_data = v2.data();
  size_t v2_size = v2.size();
  auto v2_value = Config::value(v2.get_allocator());

  ASSERT_NE(v1_value, v2_value);

  std::swap(v1, v2);

  ASSERT_EQ(v1.data(), v2_data);
  ASSERT_EQ(v1.size(), v2_size);
  ASSERT_EQ(Config::value(v1.get_allocator()), v2_value);

  ASSERT_EQ(v2.data(), v1_data);
  ASSERT_EQ(v2.size(), v1_size);
  ASSERT_EQ(Config::value(v2.get_allocator()), v1_value);
}

TEST_VM(StdAllocTestCHeapDynamic, pocs) {
  test_pocs<StdAllocTestCHeapDynamicPropagationConfig>();
}

TEST_VM(StdAllocTestArena, pocs) {
  test_pocs<StdAllocTestArenaPropagationConfig>();
}
