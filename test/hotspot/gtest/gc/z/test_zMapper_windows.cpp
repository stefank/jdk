/*
 * Copyright (c) 2024, 2025, Oracle and/or its affiliates. All rights reserved.
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
 */

#ifdef _WINDOWS

#include "gc/z/zAddress.inline.hpp"
#include "gc/z/zGlobals.hpp"
#include "gc/z/zList.inline.hpp"
#include "gc/z/zMapper_windows.hpp"
#include "gc/z/zMemory.inline.hpp"
#include "gc/z/zNUMA.inline.hpp"
#include "gc/z/zSyscall_windows.hpp"
#include "gc/z/zValue.inline.hpp"
#include "gc/z/zVirtualMemoryManager.hpp"
#include "runtime/os.hpp"
#include "unittest.hpp"

using namespace testing;

#define EXPECT_REMOVAL_OK(range) EXPECT_FALSE(range.is_null())

using ZMemoryManager = ZMemoryManagerImpl<ZVirtualMemory>;

class ZMapperTest : public Test {
private:
  static constexpr size_t ZMapperTestReservationSize = 32 * M;

  static bool             _initialized;
  static ZMemoryManager*  _va;

  static ZVirtualMemoryReserver* _vmr;

public:
  virtual void SetUp() {
    // Only run test on supported Windows versions
    if (!ZSyscall::is_supported()) {
      GTEST_SKIP() << "Requires Windows version 1803 or later";
      return;
    }

    ZSyscall::initialize();
    ZGlobalsPointers::initialize();
    ZNUMA::initialize();

    void* vmr_mem = os::malloc(sizeof(ZVirtualMemoryReserver), mtTest);
    _vmr = ::new (vmr_mem) ZVirtualMemoryReserver(ZMapperTestReservationSize);
    _va = &_vmr->_virtual_memory_reservation;

    // Reserve address space for the test
    if (_vmr->reserved() != ZMapperTestReservationSize) {
      GTEST_SKIP() << "Failed to reserve address space";
      return;
    }

    // Set up the callbacks
    _vmr->pd_register_callbacks(_va);

    _initialized = true;
  }

  virtual void TearDown() {
    if (!ZSyscall::is_supported()) {
      // Test skipped, nothing to cleanup
      return;
    }

    if (_initialized) {
      _vmr->unreserve();
    }

    _vmr->~ZVirtualMemoryReserver();
    os::free(_vmr);
  }

  static void test_unreserve() {
    ZVirtualMemory bottom = _va->remove_from_low(ZGranuleSize);
    ZVirtualMemory top    = _va->remove_from_high(ZGranuleSize);

    // Unreserve the middle part
    _vmr->unreserve();

    // Make sure that we still can unreserve the memory before and after
    ZMapper::unreserve(ZOffset::address_unsafe(bottom.start()), bottom.size());
    ZMapper::unreserve(ZOffset::address_unsafe(top.start()), top.size());
  }

  static void test_remove_from_low() {
    // Verify that we get placeholder for first granule
    ZVirtualMemory bottom = _va->remove_from_low(ZGranuleSize);
    EXPECT_REMOVAL_OK(bottom);

    _va->insert(bottom);

    // Remove something larger than a granule and insert it
    bottom = _va->remove_from_low(ZGranuleSize * 3);
    EXPECT_REMOVAL_OK(bottom);

    _va->insert(bottom);

    // Insert with more memory removed
    bottom = _va->remove_from_low(ZGranuleSize);
    EXPECT_REMOVAL_OK(bottom);

    ZVirtualMemory next = _va->remove_from_low(ZGranuleSize);
    EXPECT_REMOVAL_OK(next);

    _va->insert(bottom);
    _va->insert(next);
  }

  static void test_remove_from_high() {
    // Verify that we get placeholder for last granule
    ZVirtualMemory high = _va->remove_from_high(ZGranuleSize);
    EXPECT_REMOVAL_OK(high);

    ZVirtualMemory prev = _va->remove_from_high(ZGranuleSize);
    EXPECT_REMOVAL_OK(prev);

    _va->insert(high);
    _va->insert(prev);

    // Remove something larger than a granule and return it
    high = _va->remove_from_high(ZGranuleSize * 2);
    EXPECT_REMOVAL_OK(high);

    _va->insert(high);
  }

  static void test_remove_whole_area() {
    // Remove the whole reservation
    ZVirtualMemory bottom = _va->remove_from_low(ZMapperTestReservationSize);
    EXPECT_REMOVAL_OK(bottom);

    // Insert two chunks and then remove them again
    _va->insert({bottom.start(), ZGranuleSize * 4});
    _va->insert({bottom.start() + ZGranuleSize * 6, ZGranuleSize * 6});

    ZVirtualMemory range = _va->remove_from_low(ZGranuleSize * 4);
    EXPECT_REMOVAL_OK(range);

    range = _va->remove_from_low(ZGranuleSize * 6);
    EXPECT_REMOVAL_OK(range);

    // Now insert it all, and verify it can be removed again
    _va->insert({bottom.start(), ZMapperTestReservationSize});

    bottom = _va->remove_from_low(ZMapperTestReservationSize);
    EXPECT_REMOVAL_OK(bottom);

    _va->insert({bottom.start(), ZMapperTestReservationSize});
  }
};

bool ZMapperTest::_initialized              = false;
ZMemoryManager* ZMapperTest::_va            = nullptr;
ZVirtualMemoryReserver* ZMapperTest::_vmr   = nullptr;

TEST_VM_F(ZMapperTest, test_unreserve) {
  test_unreserve();
}

TEST_VM_F(ZMapperTest, test_remove_from_low) {
  test_remove_from_low();
}

TEST_VM_F(ZMapperTest, test_remove_from_high) {
  test_remove_from_high();
}

TEST_VM_F(ZMapperTest, test_remove_whole_area) {
  test_remove_whole_area();
}

#endif // _WINDOWS
