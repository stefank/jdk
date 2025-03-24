/*
 * Copyright (c) 2015, 2025, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_GC_Z_ZMEMORY_HPP
#define SHARE_GC_Z_ZMEMORY_HPP

#include "gc/z/zAddress.hpp"
#include "gc/z/zArray.hpp"
#include "gc/z/zList.hpp"
#include "gc/z/zLock.hpp"
#include "memory/allocation.hpp"

template <typename Range>
class ZRangeNode;

template <typename Start, typename End>
class ZRange {
  friend class VMStructs;

public:
  using offset     = Start;
  using offset_end = End;

private:
  End _start;
  End _end;

public:
  ZRange();
  ZRange(Start start, size_t size);
  ZRange(End start, size_t size);

  bool is_null() const;

  Start start() const;
  End end() const;

  size_t size() const;

  void shrink_from_front(size_t size);
  void shrink_from_back(size_t size);
  void grow_from_front(size_t size);
  void grow_from_back(size_t size);

  ZRange split_from_front(size_t size);
  ZRange split_from_back(size_t size);

  ZRange partition(size_t offset, size_t partition_size) const;
  ZRange first_part(size_t split_offset) const;
  ZRange last_part(size_t split_offset) const;

  bool adjacent_to(const ZRange& other) const;
};

class ZVirtualMemory : public ZRange<zoffset, zoffset_end> {
public:
  ZVirtualMemory();
  ZVirtualMemory(zoffset start, size_t size);
  ZVirtualMemory(const ZRange<zoffset, zoffset_end>& range);

  int granule_count() const;
};

using ZBackingIndexRange = ZRange<zbacking_index, zbacking_index_end>;

template <typename Range>
class ZMemoryManagerImpl {
private:
  using ZMemory = ZRangeNode<Range>;

public:
  using offset     = typename Range::offset;
  using offset_end = typename Range::offset_end;
  typedef void (*RangeCallback)(const Range& range);
  typedef void (*ResizeCallback)(const Range& range, size_t size);

  struct Callbacks {
    RangeCallback  _insert_stand_alone;
    ResizeCallback _insert_from_front;
    ResizeCallback _insert_from_back;

    RangeCallback  _remove_stand_alone;
    ResizeCallback _remove_from_front;
    ResizeCallback _remove_from_back;

    ResizeCallback _transfer_from_front;

    Callbacks();
  };

private:
  mutable ZLock  _lock;
  ZList<ZMemory> _list;
  Callbacks      _callbacks;
  Range          _limits;

  void insert_inner(const Range& range);

  void insert_stand_alone_last(ZMemory* area);
  void insert_stand_alone_before(ZMemory* area, ZMemory* before);
  void insert_from_front(ZMemory* area, size_t size);
  void insert_from_back(ZMemory* area, size_t size);

  Range remove_stand_alone(ZMemory* area);
  Range remove_from_front(ZMemory* area, size_t size);
  Range remove_from_back(ZMemory* area, size_t size);

  void transfer_from_front(ZMemory* area, size_t size, ZMemoryManagerImpl<Range>* other);

  Range remove_from_low_inner(size_t size);
  Range remove_from_low_at_most_inner(size_t size);

  size_t remove_from_low_many_at_most_inner(size_t size, ZArray<Range>* out);

public:
  ZMemoryManagerImpl();

  void register_callbacks(const Callbacks& callbacks);

  bool is_empty() const;
  bool is_contiguous() const;

  Range limits() const;
  void anchor_limits();
  bool limits_contain(const Range& range) const;
  bool check_limits(const Range& range) const;

  offset peek_low_address() const;

  void insert_and_remove_from_low_many(const Range& range, ZArray<Range>* out);
  Range insert_and_remove_from_low_exact_or_many(size_t size, ZArray<Range>* in_out);

  void insert(const Range& range);

  Range remove_from_low(size_t size);
  Range remove_from_low_at_most(size_t size);
  size_t remove_from_low_many_at_most(size_t size, ZArray<Range>* out);
  Range remove_from_high(size_t size);

  void transfer_from_low(ZMemoryManagerImpl* other, size_t size);

  bool disown_first(Range* out);
};

#endif // SHARE_GC_Z_ZMEMORY_HPP
