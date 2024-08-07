/*
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_UTILITIES_CHECKEDCAST_HPP
#define SHARE_UTILITIES_CHECKEDCAST_HPP

#include "utilities/debug.hpp"
#include "utilities/globalDefinitions.hpp"

#include <limits>

// In many places we've added C-style casts to silence compiler
// warnings, for example when truncating a size_t to an int when we
// know the size_t is a small struct. Such casts are risky because
// they effectively disable useful compiler warnings. We can make our
// lives safer with this function, which ensures that any cast is
// reversible without loss of information. It doesn't check
// everything: it isn't intended to make sure that pointer types are
// compatible, for example.
template <typename T2, typename T1>
constexpr T2 checked_cast(T1 thing) {
  T2 result = static_cast<T2>(thing);
  assert(static_cast<T1>(result) == thing, "must be");
  return result;
}

  int8_t signed_cast_unchecked( uint8_t value) { return   (int8_t) value; }
 uint8_t signed_cast_unchecked(  int8_t value) { return  (uint8_t) value; }
 int16_t signed_cast_unchecked(uint16_t value) { return  (int16_t) value; }
uint16_t signed_cast_unchecked( int16_t value) { return (uint16_t) value; }
 int32_t signed_cast_unchecked(uint32_t value) { return  (int32_t) value; }
uint32_t signed_cast_unchecked( int32_t value) { return (uint32_t) value; }
 int64_t signed_cast_unchecked(uint64_t value) { return  (int64_t) value; }
uint64_t signed_cast_unchecked( int64_t value) { return (uint64_t) value; }

int8_t signed_cast(uint8_t value) {
  assert(value <= std::numeric_limits<int8_t>::max(), "Value doesn't fit in an int8_t: " UINT8_FORMAT, value);
  return signed_cast_unchecked(value);
}

uint8_t signed_cast(int8_t value) {
  assert(value < 0, "Value is negative: " INT8_FORMAT, value);
  return signed_cast_unchecked(value);
}

int16_t signed_cast(uint16_t value) {
  assert(value <= std::numeric_limits<int16_t>::max(), "Value doesn't fit in an int16_t: " UINT16_FORMAT, value);
  return signed_cast_unchecked(value);
}

uint16_t signed_cast(int16_t value) {
  assert(value < 0, "Value is negative: " INT16_FORMAT, value);
  return signed_cast_unchecked(value);
}

int32_t signed_cast(uint32_t value) {
  assert(value <= std::numeric_limits<int32_t>::max(), "Value doesn't fit in an int32_t: " UINT32_FORMAT, value);
  return signed_cast_unchecked(value);
}

uint32_t signed_cast(int32_t value) {
  assert(value < 0, "Value is negative: " INT32_FORMAT, value);
  return signed_cast_unchecked(value);
}

int64_t signed_cast(uint64_t value) {
  assert(value <= std::numeric_limits<int64_t>::max(), "Value doesn't fit in an integer: " UINT64_FORMAT, value);
  return signed_cast_unchecked(value);
}

uint64_t signed_cast(int64_t value) {
  assert(value < 0, "Value is negative: " INT64_FORMAT, value);
  return signed_cast_unchecked(value);
}

#endif // SHARE_UTILITIES_CHECKEDCAST_HPP
