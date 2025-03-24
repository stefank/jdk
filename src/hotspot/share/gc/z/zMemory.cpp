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

#include "gc/z/zAddress.inline.hpp"
#include "gc/z/zArray.inline.hpp"
#include "gc/z/zList.inline.hpp"
#include "gc/z/zLock.inline.hpp"
#include "gc/z/zMemory.inline.hpp"

template <typename Range>
class ZRangeNode : public CHeapObj<mtGC> {
  friend class ZList<ZRangeNode>;

private:
  using offset     = typename Range::offset;
  using offset_end = typename Range::offset_end;

  Range                 _range;
  ZListNode<ZRangeNode> _node;

public:
  ZRangeNode(offset start, size_t size)
    : _range(start, size),
      _node() {}

  Range* range() {
    return &_range;
  }

  offset start() const {
    return _range.start();
  }

  offset_end end() const {
    return _range.end();
  }

  size_t size() const {
    return _range.size();
  }
};

template <typename Range>
void ZMemoryManagerImpl<Range>::insert_inner(const Range& range) {
  assert(!range.is_null(), "Invalid range");
  assert(check_limits(range), "Range outside limits");

  const offset start = range.start();
  const offset_end end = range.end();
  const size_t size = range.size();

  ZListIterator<ZMemory> iter(&_list);
  for (ZMemory* area; iter.next(&area);) {
    if (area->start() < start) {
      continue;
    }

    ZMemory* const prev = _list.prev(area);
    if (prev != nullptr && start == prev->end()) {
      if (end == area->start()) {
        // Merge with prev and current area
        insert_from_back(prev, size + area->size());
        _list.remove(area);
        delete area;
      } else {
        // Merge with prev area
        insert_from_back(prev, size);
      }
    } else if (end == area->start()) {
      // Merge with current area
      insert_from_front(area, size);
    } else {
      // Insert new area before current area
      assert(end < area->start(), "Areas must not overlap");
      ZMemory* const new_area = new ZMemory(start, size);
      insert_stand_alone_before(new_area, area);
    }

    // Done
    return;
  }

  // Insert last
  ZMemory* const last = _list.last();
  if (last != nullptr && start == last->end()) {
    // Merge with last area
    insert_from_back(last, size);
  } else {
    // Insert new area last
    ZMemory* const new_area = new ZMemory(start, size);
    insert_stand_alone_last(new_area);
  }
}

template <typename Range>
void ZMemoryManagerImpl<Range>::insert_stand_alone_last(ZMemory* area) {
  if (_callbacks._insert_stand_alone != nullptr) {
    _callbacks._insert_stand_alone(*area->range());
  }
  _list.insert_last(area);
}

template <typename Range>
void ZMemoryManagerImpl<Range>::insert_stand_alone_before(ZMemory* area, ZMemory* before) {
  if (_callbacks._insert_stand_alone != nullptr) {
    _callbacks._insert_stand_alone(*area->range());
  }
  _list.insert_before(before, area);
}

template <typename Range>
void ZMemoryManagerImpl<Range>::insert_from_front(ZMemory* area, size_t size) {
  if (_callbacks._insert_from_front != nullptr) {
    _callbacks._insert_from_front(*area->range(), size);
  }
  area->range()->grow_from_front(size);
}

template <typename Range>
void ZMemoryManagerImpl<Range>::insert_from_back(ZMemory* area, size_t size) {
  if (_callbacks._insert_from_back != nullptr) {
    _callbacks._insert_from_back(*area->range(), size);
  }
  area->range()->grow_from_back(size);
}

template <typename Range>
Range ZMemoryManagerImpl<Range>::remove_stand_alone(ZMemory* area) {
  if (_callbacks._remove_stand_alone != nullptr) {
    _callbacks._remove_stand_alone(*area->range());
  }
  _list.remove(area);
  return *area->range();
}

template <typename Range>
Range ZMemoryManagerImpl<Range>::remove_from_front(ZMemory* area, size_t size) {
  if (_callbacks._remove_from_front != nullptr) {
    _callbacks._remove_from_front(*area->range(), size);
  }
  return area->range()->split_from_front(size);
}

template <typename Range>
Range ZMemoryManagerImpl<Range>::remove_from_back(ZMemory* area, size_t size) {
  if (_callbacks._remove_from_back != nullptr) {
    _callbacks._remove_from_back(*area->range(), size);
  }
  return area->range()->split_from_back(size);
}

template <typename Range>
void ZMemoryManagerImpl<Range>::transfer_from_front(ZMemory* area, size_t size, ZMemoryManagerImpl<Range>* other) {
  if (_callbacks._transfer_from_front != nullptr) {
    _callbacks._transfer_from_front(*area->range(), size);
  }
  Range to_transfer =  area->range()->split_from_front(size);
  other->insert(to_transfer);
}

template <typename Range>
Range ZMemoryManagerImpl<Range>::remove_from_low_inner(size_t size) {
  ZListIterator<ZMemory> iter(&_list);
  for (ZMemory* area; iter.next(&area);) {
    if (area->size() >= size) {
      if (area->size() == size) {
        // Exact match, remove area
        const Range range = remove_stand_alone(area);
        delete area;
        return range;
      } else {
        // Larger than requested, shrink area
        return remove_from_front(area, size);
      }
    }
  }

  // Out of memory
  return Range();
}

template <typename Range>
Range ZMemoryManagerImpl<Range>::remove_from_low_at_most_inner(size_t size) {
  ZMemory* const area = _list.first();
  if (area != nullptr) {
    if (area->size() <= size) {
      // Smaller than or equal to requested, remove area
      const Range range = remove_stand_alone(area);
      delete area;
      return range;
    } else {
      // Larger than requested, shrink area
      return remove_from_front(area, size);
    }
  }

  return Range();
}

template <typename Range>
size_t ZMemoryManagerImpl<Range>::remove_from_low_many_at_most_inner(size_t size, ZArray<Range>* out) {
  size_t to_remove = size;

  while (to_remove > 0) {
    const Range range = remove_from_low_at_most_inner(to_remove);

    if (range.is_null()) {
      // The requested amount is not available
      return size - to_remove;
    }

    to_remove -= range.size();
    out->append(range);
  }

  return size;
}

template <typename Range>
ZMemoryManagerImpl<Range>::Callbacks::Callbacks()
  : _insert_stand_alone(nullptr),
    _insert_from_front(nullptr),
    _insert_from_back(nullptr),
    _remove_stand_alone(nullptr),
    _remove_from_front(nullptr),
    _remove_from_back(nullptr),
    _transfer_from_front(nullptr) {}

template <typename Range>
ZMemoryManagerImpl<Range>::ZMemoryManagerImpl()
  : _list(),
    _callbacks(),
    _limits() {}

template <typename Range>
void ZMemoryManagerImpl<Range>::register_callbacks(const Callbacks& callbacks) {
  _callbacks = callbacks;
}

template <typename Range>
bool ZMemoryManagerImpl<Range>::is_empty() const {
  return _list.is_empty();
}

template <typename Range>
bool ZMemoryManagerImpl<Range>::is_contiguous() const {
  return _list.size() == 1;
}

template <typename Range>
Range ZMemoryManagerImpl<Range>::limits() const {
  assert(!_limits.is_null(), "Limits not anchored");
  return _limits;
}

template <typename Range>
void ZMemoryManagerImpl<Range>::anchor_limits() {
  assert(_limits.is_null(), "Should only anchor limits once");

  if (_list.is_empty()) {
    return;
  }

  const offset start = _list.first()->start();
  const size_t size = _list.last()->end() - start;

  _limits = Range(start, size);
 }

template <typename Range>
bool ZMemoryManagerImpl<Range>::limits_contain(const Range& range) const {
  if (_limits.is_null() || range.is_null()) {
    return false;
  }

  return range.start() >= _limits.start() && range.end() <= _limits.end();
}

template <typename Range>
bool ZMemoryManagerImpl<Range>::check_limits(const Range& range) const {
  if (_limits.is_null()) {
    // Limits not anchored
    return true;
  }

  // Otherwise, check that other is within the limits
  return limits_contain(range);
 }

template <typename Range>
typename ZMemoryManagerImpl<Range>::offset ZMemoryManagerImpl<Range>::peek_low_address() const {
  ZLocker<ZLock> locker(&_lock);

  const ZMemory* const area = _list.first();
  if (area != nullptr) {
    return area->start();
  }

  // Out of memory
  return offset::invalid;
}

template <typename Range>
void ZMemoryManagerImpl<Range>::insert(const Range& range) {
  ZLocker<ZLock> locker(&_lock);
  insert_inner(range);
}

template <typename Range>
void ZMemoryManagerImpl<Range>::insert_and_remove_from_low_many(const Range& range, ZArray<Range>* out) {
  ZLocker<ZLock> locker(&_lock);

  const size_t size = range.size();

  // Insert the range
  insert_inner(range);

  // Remove (hopefully) at a lower address
  const size_t removed = remove_from_low_many_at_most_inner(size, out);

  // This should always succeed since we freed the same amount.
  assert(removed == size, "must succeed");
}

template <typename Range>
Range ZMemoryManagerImpl<Range>::insert_and_remove_from_low_exact_or_many(size_t size, ZArray<Range>* in_out) {
  ZLocker<ZLock> locker(&_lock);

  size_t inserted = 0;

  // Insert everything
  ZArrayIterator<Range> iter(in_out);
  for (Range mem; iter.next(&mem);) {
    insert_inner(mem);
    inserted += mem.size();
  }

  // Clear stored memory so that we can populate it below
  in_out->clear();

  // Try to find and remove a contiguous chunk
  Range range = remove_from_low_inner(size);
  if (!range.is_null()) {
    return range;
  }

  // Failed to find a contiguous chunk, split it up into smaller chunks and
  // only remove up to as much that has been inserted.
  size_t removed = remove_from_low_many_at_most_inner(inserted, in_out);
  assert(removed == inserted, "Should be able to get back as much as we previously inserted");
  return Range();
}

template <typename Range>
Range ZMemoryManagerImpl<Range>::remove_from_low(size_t size) {
  ZLocker<ZLock> locker(&_lock);
  Range range = remove_from_low_inner(size);
  return range;
}

template <typename Range>
Range ZMemoryManagerImpl<Range>::remove_from_low_at_most(size_t size) {
  ZLocker<ZLock> lock(&_lock);
  Range range = remove_from_low_at_most_inner(size);
  return range;
}

template <typename Range>
size_t ZMemoryManagerImpl<Range>::remove_from_low_many_at_most(size_t size, ZArray<Range>* out) {
  ZLocker<ZLock> lock(&_lock);
  return remove_from_low_many_at_most_inner(size, out);
}

template <typename Range>
Range ZMemoryManagerImpl<Range>::remove_from_high(size_t size) {
  ZLocker<ZLock> locker(&_lock);

  ZListReverseIterator<ZMemory> iter(&_list);
  for (ZMemory* area; iter.next(&area);) {
    if (area->size() < size) {
      continue;
    }

    if (area->size() == size) {
      // Exact match, remove area
      const Range range = remove_stand_alone(area);
      delete area;
      return range;
    } else {
      // Larger than requested, shrink area
      return remove_from_back(area, size);
    }
  }

  // Out of memory
  return Range();
}

template <typename Range>
void ZMemoryManagerImpl<Range>::transfer_from_low(ZMemoryManagerImpl* other, size_t size) {
  assert(other->_list.is_empty(), "Should only be used for initialization");

  ZLocker<ZLock> locker(&_lock);
  size_t to_move = size;

  ZListIterator<ZMemory> iter(&_list);
  for (ZMemory* area; iter.next(&area);) {
    if (area->size() <= to_move) {
      // Smaller than or equal to requested, remove from this list and
      // insert in other's list
      to_move -= area->size();
      _list.remove(area);
      other->_list.insert_last(area);
    } else {
      // Larger than requested, shrink area
      transfer_from_front(area, to_move, other);
      to_move = 0;
    }

    if (to_move == 0) {
      break;
    }
  }

  assert(to_move == 0, "Should have transferred requested size");
}

template <typename Range>
bool ZMemoryManagerImpl<Range>::disown_first(Range* out) {
  // This intentionally does not call the "remove" callback.
  // This call is typically used to disown memory before unreserving a surplus.

  ZLocker<ZLock> locker(&_lock);

  if (_list.is_empty()) {
    return false;
  }

  // Don't invoke the "remove" callback

  ZMemory* const area = _list.remove_first();

  // Return the range
  *out = *area->range();

  delete area;

  return true;
}

// Instantiate the concrete classes
template class ZMemoryManagerImpl<ZVirtualMemory>;
template class ZMemoryManagerImpl<ZBackingIndexRange>;
