/*
 * Copyright (c) 2000, 2023, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_UTILITIES_SIZES_HPP
#define SHARE_UTILITIES_SIZES_HPP

#include "utilities/globalDefinitions.hpp"

// The following classes are used to represent 'sizes' and 'offsets' in the VM;
// they serve as 'unit' types. BytesInt is used for sizes measured in bytes, while
// WordsInt is used for sizes measured in machine words (i.e., 32bit or 64bit words
// depending on platform).

enum class WordsInt : int {};

constexpr WordsInt in_WordsInt(int size) { return static_cast<WordsInt>(size); }
constexpr int      in_words(WordsInt x)  { return static_cast<int>(x); }

enum class BytesInt : int {};

constexpr BytesInt in_BytesInt(int size) { return static_cast<BytesInt>(size); }
constexpr int      in_bytes(BytesInt x)  { return static_cast<int>(x); }

constexpr BytesInt operator + (BytesInt x, BytesInt y) { return in_BytesInt(in_bytes(x) + in_bytes(y)); }
constexpr BytesInt operator - (BytesInt x, BytesInt y) { return in_BytesInt(in_bytes(x) - in_bytes(y)); }
constexpr BytesInt operator * (BytesInt x, int      y) { return in_BytesInt(in_bytes(x) * y          ); }

constexpr bool     operator == (BytesInt x, int     y) { return in_bytes(x) == y; }
constexpr bool     operator != (BytesInt x, int     y) { return in_bytes(x) != y; }

// Use the following #define to get C++ field member offsets

#define byte_offset_of(klass,field)   in_BytesInt((int)offset_of(klass, field))

#endif // SHARE_UTILITIES_SIZES_HPP
