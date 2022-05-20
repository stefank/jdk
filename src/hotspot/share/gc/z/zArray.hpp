/*
 * Copyright (c) 2015, 2020, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_GC_Z_ZARRAY_HPP
#define SHARE_GC_Z_ZARRAY_HPP

#include "memory/allocation.hpp"
#include "utilities/cHeapVector.hpp"
#include "utilities/growableArray.hpp"

template <typename T> using ZArray = CHeapVector<T, mtGC>;

template <bool Parallel>
class ZRangeIteratorImpl : public StackObj {
private:
  size_t _next;
  size_t _end;

  bool next_serial(size_t* index);
  bool next_parallel(size_t* index);

public:
  ZRangeIteratorImpl(size_t begin, size_t end);

  bool next(size_t* index);
};

template <typename T, bool Parallel>
class ZArrayIteratorImpl : public StackObj {
  const ZArray<T>&             _array;
  ZRangeIteratorImpl<Parallel> _range_iter;

public:
  ZArrayIteratorImpl(const ZArray<T>& array);

  bool next(T* elem);
};

template <typename T, bool Parallel>
class ZCArrayIteratorImpl : public StackObj {
  const T*                     _array;
  ZRangeIteratorImpl<Parallel> _range_iter;

public:
  ZCArrayIteratorImpl(const T* array, size_t length);

  bool next(T* elem);
};

template <typename T> using ZArrayIterator = ZArrayIteratorImpl<T, false /* Parallel */>;
template <typename T> using ZArrayParallelIterator = ZArrayIteratorImpl<T, true /* Parallel */>;

template <typename T> using ZCArrayIterator = ZCArrayIteratorImpl<T, false /* Parallel */>;
template <typename T> using ZCArrayParallelIterator = ZCArrayIteratorImpl<T, true /* Parallel */>;

#endif // SHARE_GC_Z_ZARRAY_HPP
