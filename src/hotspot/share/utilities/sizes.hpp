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

// The following classes are used to represent 'sizes' and 'offsets' in the VM;
// they serve as 'unit' types. BytesInt is used for sizes measured in bytes, while
// WordsInt is used for sizes measured in machine words (i.e., 32bit or 64bit words
// depending on platform).

//
// Words is a wrapper for a size_t-typed count of words
//

enum class Words : size_t {};

constexpr Words      in_Words(size_t x)           { return static_cast<Words>(x); }
constexpr size_t       untype(Words x)            { return static_cast<size_t>(x); }

constexpr Words operator +   (Words x, Words  y)  { return in_Words(untype(x) + untype(y)); }
constexpr Words operator -   (Words x, Words  y)  { return in_Words(untype(x) - untype(y)); }
constexpr Words operator *   (Words x, size_t y)  { return in_Words(untype(x) *        y ); }
constexpr Words operator *   (size_t x, Words y)  { return in_Words(       x  * untype(y)); }
constexpr Words operator /   (Words x, size_t y)  { return in_Words(untype(x) /        y) ; }

constexpr size_t operator /  (Words x, Words y)   { return untype(x) / untype(y); }

constexpr Words& operator += (Words& x, Words  y) { x = x + y; return x; }
constexpr Words& operator -= (Words& x, Words  y) { x = x - y; return x; }
constexpr Words& operator *= (Words& x, size_t y) { x = x * y; return x; }
constexpr Words& operator /= (Words& x, size_t y) { x = x / y; return x; }

constexpr Words& operator ++ (Words& x)           { x = x + Words(1); return x;}
constexpr Words& operator -- (Words& x)           { x = x - Words(1); return x;}
constexpr Words  operator ++ (Words& x, int)      { Words pre = x; x = x + Words(1); return pre;}
constexpr Words  operator -- (Words& x, int)      { Words pre = x; x = x - Words(1); return pre;}

// Helpers to manipulate pointers with Words. Useful for HeapWord* and MetaWord*,
// which are actually HeapWordImpl** and MetaWordImpl**.

template <typename T>
T*const* operator +  (T*const* ptr, Words v) { return ptr + untype(v); }
template <typename T>
T**      operator +  (T**      ptr, Words v) { return ptr + untype(v); }
template <typename T>
T**      operator - (T**       ptr, Words v) { return ptr - untype(v); }
template <typename T>
T*const* operator - (T*const*  ptr, Words v) { return ptr - untype(v); }
template <typename T>

T**&     operator+=(T**&       ptr, Words v) { ptr += untype(v); return ptr; }
template <typename T>
T**&     operator-=(T**&       ptr, Words v) { ptr -= untype(v); return ptr; }
template <typename T>
T*const*& operator+=(T*const*& ptr, Words v) { ptr += untype(v); return ptr; }
template <typename T>
T*const*& operator-=(T*const*& ptr, Words v) { ptr -= untype(v); return ptr; }

//
// Bytes is a wrapper for a size_t-typed count of bytes
//

enum class Bytes : size_t {};

constexpr Bytes      in_Bytes(size_t x)           { return static_cast<Bytes>(x); }
constexpr size_t       untype(Bytes x)            { return static_cast<size_t>(x); }

constexpr Bytes operator +   (Bytes x, Bytes  y)  { return in_Bytes(untype(x) + untype(y)); }
constexpr Bytes operator -   (Bytes x, Bytes  y)  { return in_Bytes(untype(x) - untype(y)); }
constexpr Bytes operator *   (Bytes x, size_t y)  { return in_Bytes(untype(x) *        y ); }
constexpr Bytes operator *   (size_t x, Bytes y)  { return in_Bytes(       x  * untype(y)); }
constexpr Bytes operator /   (Bytes x, size_t y)  { return in_Bytes(untype(x) /        y) ; }

constexpr size_t operator /  (Bytes x, Bytes y)   { return untype(x) / untype(y); }

constexpr Bytes& operator += (Bytes& x, Bytes  y) { x = x + y; return x; }
constexpr Bytes& operator -= (Bytes& x, Bytes  y) { x = x - y; return x; }
constexpr Bytes& operator *= (Bytes& x, size_t y) { x = x * y; return x; }
constexpr Bytes& operator /= (Bytes& x, size_t y) { x = x / y; return x; }

constexpr Bytes& operator ++ (Bytes& x)           { x = x + Bytes(1); return x;}
constexpr Bytes& operator -- (Bytes& x)           { x = x - Bytes(1); return x;}
constexpr Bytes  operator ++ (Bytes& x, int)      { Bytes pre = x; x = x + Bytes(1); return pre;}
constexpr Bytes  operator -- (Bytes& x, int)      { Bytes pre = x; x = x - Bytes(1); return pre;}

// Helpers to manipulate pointers with Bytes. Useful for HeapWord* and MetaWord*,
// which are actually HeapWordImpl** and MetaWordImpl**.

// The pointer manipulation functions are located in globalDefinition.hpp

//
//
// The below are the legacy types that wrap an int instead of size_t. This is still used a lot in
// compiler code, and therefore has been left for a future, potential cleanup.
//
//

//
// WordsInt is a wrapper for an int-typed count of words
//

enum class WordsInt : int {};

constexpr WordsInt in_WordsInt(int count)              { return static_cast<WordsInt>(count); }
constexpr int          untype(WordsInt x)              { return static_cast<int>(x); }

constexpr WordsInt operator + (WordsInt x, WordsInt y) { return in_WordsInt(untype(x) + untype(y)); }
constexpr WordsInt operator - (WordsInt x, WordsInt y) { return in_WordsInt(untype(x) - untype(y)); }
constexpr WordsInt operator * (WordsInt x, int      y) { return in_WordsInt(untype(x) * y          ); }

constexpr bool    operator == (WordsInt x, int y)      { return untype(x) == y; }
constexpr bool    operator != (WordsInt x, int y)      { return untype(x) != y; }


//
// Bytes is a wrapper for an int-typed count of bytes
//

enum class BytesInt : int {};

constexpr BytesInt in_BytesInt(int count)              { return static_cast<BytesInt>(count); }
constexpr int          untype(BytesInt x)              { return static_cast<int>(x); }
// Legacy support
constexpr int         in_bytes(BytesInt x)             { return untype(x); }

constexpr BytesInt operator + (BytesInt x, BytesInt y) { return in_BytesInt(in_bytes(x) + in_bytes(y)); }
constexpr BytesInt operator - (BytesInt x, BytesInt y) { return in_BytesInt(in_bytes(x) - in_bytes(y)); }
constexpr BytesInt operator * (BytesInt x, int      y) { return in_BytesInt(in_bytes(x) * y          ); }

constexpr bool    operator == (BytesInt x, int y)      { return in_bytes(x) == y; }
constexpr bool    operator != (BytesInt x, int y)      { return in_bytes(x) != y; }

// Use the following #define to get C++ field member offsets

#define byte_offset_of(klass,field)   in_BytesInt((int)offset_of(klass, field))

#endif // SHARE_UTILITIES_SIZES_HPP
