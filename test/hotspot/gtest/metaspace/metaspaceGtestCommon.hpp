/*
 * Copyright (c) 2020, 2024, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2020 SAP SE. All rights reserved.
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

#ifndef GTEST_METASPACE_METASPACEGTESTCOMMON_HPP
#define GTEST_METASPACE_METASPACEGTESTCOMMON_HPP

#include "memory/allocation.hpp"
#include "runtime/os.hpp"
#include "utilities/globalDefinitions.hpp"
#include "unittest.hpp"

/////////////////////////////////////////////////////////////////////
// A little mockup to mimick and test the CommitMask in various tests

class TestMap {
  const Words _len;
  char* _arr;
public:
  TestMap(Words len) : _len(len), _arr(nullptr) {
    _arr = NEW_C_HEAP_ARRAY(char, untype(len), mtInternal);
    memset(_arr, 0, untype(_len));
  }
  ~TestMap() { FREE_C_HEAP_ARRAY(char, _arr); }

  int get_num_set(Words from, Words to) const {
    int result = 0;
    for(Words i = from; i < to; i++) {
      if (_arr[untype(i)] > 0) {
        result++;
      }
    }
    return result;
  }

  int get_num_set() const { return get_num_set(Words(0), _len); }

  void set_range(Words from, Words to) {
    memset(_arr + untype(from), 1, untype(to - from));
  }

  void clear_range(Words from, Words to) {
    memset(_arr + untype(from), 0, untype(to - from));
  }

  bool at(Words pos) const {
    return _arr[untype(pos)] == 1;
  }

};

///////////////////////////////////////////////////////////
// Helper class for generating random allocation sizes
class RandSizeGenerator {
  const Words _min; // [
  const Words _max; // )
  const float _outlier_chance; // 0.0 -- 1.0
  const Words _outlier_min; // [
  const Words _outlier_max; // )
public:
  RandSizeGenerator(Words min, Words max) :
    _min(min),
    _max(max),
    _outlier_chance(0.0),
    _outlier_min(min),
    _outlier_max(max)
  {}

  RandSizeGenerator(Words min, Words max, float outlier_chance, Words outlier_min, Words outlier_max) :
    _min(min),
    _max(max),
    _outlier_chance(outlier_chance),
    _outlier_min(outlier_min),
    _outlier_max(outlier_max)
  {}

  Words min() const { return _min; }
  Words max() const { return _max; }

  Words get() const {
    Words l1 = _min;
    Words l2 = _max;
    int r = os::random() % 1000;
    if ((float)r < _outlier_chance * 1000.0) {
      l1 = _outlier_min;
      l2 = _outlier_max;
    }
    const Words d = l2 - l1;
    return l1 + in_Words(os::random() % untype(d));
  }

}; // end RandSizeGenerator

Words get_random_size(Words min, Words max);

///////////////////////////////////////////////////////////
// Function to test-access a memory range

void zap_range(MetaWord* p, Words word_size);

// "fill_range_with_pattern" fills a range of heap words with pointers to itself.
//
// The idea is to fill a memory range with a pattern which is both marked clearly to the caller
// and cannot be moved without becoming invalid.
//
// The filled range can be checked with check_range_for_pattern. One also can only check
// a sub range of the original range.
void fill_range_with_pattern(MetaWord* p, Words word_size, uintx pattern);
void check_range_for_pattern(const MetaWord* p, Words word_size, uintx pattern);

// Writes a uniqe pattern to p
void mark_address(MetaWord* p, uintx pattern);
// checks pattern at address
void check_marked_address(const MetaWord* p, uintx pattern);

// Similar to fill_range_with_pattern, but only marks start and end. This is optimized for cases
// where fill_range_with_pattern just is too slow.
// Use check_marked_range to check the range. In contrast to check_range_for_pattern, only the original
// range can be checked.
void mark_range(MetaWord* p, uintx pattern, Words word_size);
void check_marked_range(const MetaWord* p, uintx pattern, Words word_size);

void mark_range(MetaWord* p, Words word_size);
void check_marked_range(const MetaWord* p, Words word_size);

//////////////////////////////////////////////////////////
// Some helpers to avoid typing out those annoying casts for nullptr

#define ASSERT_NOT_NULL(ptr)      ASSERT_NE(nullptr, (void*)ptr)
#define ASSERT_NULL(ptr)          ASSERT_EQ(nullptr, (void*)ptr)
#define EXPECT_NOT_NULL(ptr)      EXPECT_NE(nullptr, (void*)ptr)
#define EXPECT_NULL(ptr)          EXPECT_EQ(nullptr, (void*)ptr)

#define ASSERT_0(v)               ASSERT_EQ((intptr_t)0, (intptr_t)v)
#define ASSERT_NOT_0(v)           ASSERT_NE((intptr_t)0, (intptr_t)v)
#define EXPECT_0(v)               EXPECT_EQ((intptr_t)0, (intptr_t)v)
#define EXPECT_NOT_0(v)           EXPECT_NE((intptr_t)0, (intptr_t)v)
#define ASSERT_GT0(v)             ASSERT_GT((intptr_t)v, (intptr_t)0)
#define EXPECT_GT0(v)             EXPECT_GT((intptr_t)v, (intptr_t)0)

//////////////////////////////////////////////////////////
// logging

// Define "LOG_PLEASE" to switch on logging for a particular test before inclusion of this header.
#ifdef LOG_PLEASE
  #define LOG(...) { printf(__VA_ARGS__); printf("\n"); fflush(stdout); }
#else
  #define LOG(...)
#endif

//////////////////////////////////////////////////////////
// Helper

size_t get_workingset_size();

// A simple preallocated buffer used to "feed" someone.
// Mimicks chunk retirement leftover blocks.
class FeederBuffer {

  MetaWord* _buf;

  // Buffer capacity in size of words.
  const Words _cap;

  // Used words.
  Words _used;

public:

  FeederBuffer(Words size) : _buf(nullptr), _cap(size), _used(Words(0)) {
    _buf = NEW_C_HEAP_ARRAY(MetaWord, untype(_cap), mtInternal);
  }

  ~FeederBuffer() {
    FREE_C_HEAP_ARRAY(MetaWord, _buf);
  }

  MetaWord* get(Words word_size) {
    if (_used + word_size > _cap) {
      return nullptr;
    }
    MetaWord* p = _buf + _used;
    _used += word_size;
    return p;
  }

  bool is_valid_pointer(MetaWord* p) const {
    return p >= _buf && p < _buf + _used;
  }

  bool is_valid_range(MetaWord* p, Words word_size) const {
    return is_valid_pointer(p) &&
           word_size > Words(0) ? is_valid_pointer(p + word_size - 1) : true;
  }

};

#endif // GTEST_METASPACE_METASPACEGTESTCOMMON_HPP
