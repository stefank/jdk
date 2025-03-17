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

#include "gc/z/zAddress.inline.hpp"
#include "gc/z/zGlobals.hpp"
#include "gc/z/zIntrusiveRBTree.inline.hpp"
#include "gc/z/zMappedCache.hpp"
#include "gc/z/zList.inline.hpp"
#include "gc/z/zMemory.inline.hpp"
#include "utilities/align.hpp"
#include "utilities/debug.hpp"
#include "utilities/globalDefinitions.hpp"
#include "utilities/powerOfTwo.hpp"

constexpr size_t ZMappedCache::SizeClasses[];

class ZMappedCacheEntry {
private:
  zoffset                         _start;
  ZMappedCache::TreeNode          _tree_node;
  ZMappedCache::SizeClassListNode _size_class_list_node;

public:
  ZMappedCacheEntry(zoffset start)
    : _start(start),
      _tree_node(),
      _size_class_list_node() {}

  static ZMappedCacheEntry* cast_to_entry(ZMappedCache::TreeNode* tree_node);
  static const ZMappedCacheEntry* cast_to_entry(const ZMappedCache::TreeNode* tree_node);
  static ZMappedCacheEntry* cast_to_entry(ZMappedCache::SizeClassListNode* list_node);

  zoffset start() const {
    return _start;
  }

  zoffset_end end() const {
    const uintptr_t this_addr = reinterpret_cast<uintptr_t>(this);
    return zoffset_end(align_up(this_addr, ZGranuleSize) - ZAddressHeapBase);
  }

  ZVirtualMemory vmem() const {
    return ZVirtualMemory(start(), end() - start());
  }

  ZMappedCache::TreeNode* node_addr() {
    return &_tree_node;
  }

  void update_start(zoffset start) {
    _start = start;
  }

  ZMappedCache::ZSizeClassListNode* size_class_node() {
    return &_size_class_list_node;
  }
};

ZMappedCacheEntry* ZMappedCacheEntry::cast_to_entry(ZMappedCache::TreeNode* tree_node) {
  return const_cast<ZMappedCacheEntry*>(ZMappedCacheEntry::cast_to_entry(const_cast<const ZMappedCache::TreeNode*>(tree_node)));
}

const ZMappedCacheEntry* ZMappedCacheEntry::cast_to_entry(const ZMappedCache::TreeNode* tree_node) {
  return (const ZMappedCacheEntry*)((uintptr_t)tree_node - offset_of(ZMappedCacheEntry, _tree_node));
}

ZMappedCacheEntry* ZMappedCacheEntry::cast_to_entry(ZMappedCache::SizeClassListNode* list_node) {
  const size_t size_class_list_nodes_offset = offset_of(ZMappedCacheEntry, _size_class_list_node);
  return (ZMappedCacheEntry*)((uintptr_t)list_node - size_class_list_nodes_offset);
}

static void* entry_address_for_zoffset_end(zoffset_end offset) {
  STATIC_ASSERT(ZCacheLineSize % alignof(ZMappedCacheEntry) == 0);

  constexpr size_t cache_lines_per_z_granule = ZGranuleSize / ZCacheLineSize;
  constexpr size_t cache_lines_per_entry = sizeof(ZMappedCacheEntry) / ZCacheLineSize +
                                           static_cast<size_t>(sizeof(ZMappedCacheEntry) % ZCacheLineSize != 0);

  // Do not use the last location
  constexpr size_t number_of_locations = cache_lines_per_z_granule / cache_lines_per_entry - 1;
  const size_t index = (untype(offset) >> ZGranuleSizeShift) % number_of_locations;
  const uintptr_t end_addr = untype(offset) + ZAddressHeapBase;

  return reinterpret_cast<void*>(end_addr - (cache_lines_per_entry * ZCacheLineSize) * (index + 1));
}

static ZMappedCacheEntry* create_entry(const ZVirtualMemory& vmem) {
  precond(vmem.size() >= ZGranuleSize);

  void* placement_addr = entry_address_for_zoffset_end(vmem.end());
  ZMappedCacheEntry* entry = new (placement_addr) ZMappedCacheEntry(vmem.start());

  assert(entry->start() == vmem.start(), "must be");
  assert(entry->end() == vmem.end(), "must be");

  return entry;
}

int ZMappedCache::EntryCompare::operator()(ZMappedCache::TreeNode* a, ZMappedCache::TreeNode* b) {
  const ZVirtualMemory vmem_a = ZMappedCacheEntry::cast_to_entry(a)->vmem();
  const ZVirtualMemory vmem_b = ZMappedCacheEntry::cast_to_entry(b)->vmem();

  if (vmem_a.end() < vmem_b.start()) { return -1; }
  if (vmem_b.end() < vmem_a.start()) { return 1; }

  return 0; // Overlapping
}

int ZMappedCache::EntryCompare::operator()(zoffset key, ZMappedCache::TreeNode* node) {
  const ZVirtualMemory vmem = ZMappedCacheEntry::cast_to_entry(node)->vmem();

  if (key < vmem.start()) { return -1; }
  if (key > vmem.end()) { return 1; }

  return 0; // Containing
}

int ZMappedCache::size_class_index(size_t size) {
  // Returns the size class index of for size, or -1 if smaller than the smallest size class.
  const int size_class_power = log2i_graceful(size) - ZGranuleSizeShift;

  if (size_class_power < MinSizeClassShift) {
    // Allocation is smaller than the smallest size class minimum size.
    return -1;
  }

  return MIN2(size_class_power, MaxSizeClassShift) - MinSizeClassShift;
}

int ZMappedCache::guaranteed_size_class_index(size_t size) {
  // Returns the size class index of the smallest size class which can always
  // accommodate a size allocation, or -1 otherwise.
  const int size_class_power = log2i_ceil(size) - ZGranuleSizeShift;

  if (size_class_power > MaxSizeClassShift) {
    // Allocation is larger than the largest size class minimum size.
    return -1;
  }

  return MAX2(size_class_power, MinSizeClassShift) - MinSizeClassShift;
}

void ZMappedCache::tree_insert(const Tree::FindCursor& cursor, const ZVirtualMemory& vmem) {
  ZMappedCacheEntry* entry = create_entry(vmem);

  // Insert in tree
  _tree.insert(entry->node_addr(), cursor);

  // And in size class lists
  const size_t size = vmem.size();
  const int index = size_class_index(size);
  if (index != -1) {
    _size_class_lists[index].insert_first(entry->size_class_node());
  }
}

void ZMappedCache::tree_remove(const Tree::FindCursor& cursor, const ZVirtualMemory& vmem) {
  ZMappedCacheEntry* entry = ZMappedCacheEntry::cast_to_entry(cursor.node());

  // Remove from tree
  _tree.remove(cursor);

  // And in size class lists
  const size_t size = vmem.size();
  const int index = size_class_index(size);
  if (index != -1) {
    _size_class_lists[index].remove(entry->size_class_node());
  }

  // Destroy entry
  entry->~ZMappedCacheEntry();
}

void ZMappedCache::tree_replace(const Tree::FindCursor& cursor, const ZVirtualMemory& vmem) {
  ZMappedCacheEntry* entry = create_entry(vmem);

  ZMappedCache::TreeNode* const node = cursor.node();
  ZMappedCacheEntry* old_entry = ZMappedCacheEntry::cast_to_entry(node);
  assert(old_entry->end() != vmem.end(), "should not replace, use update");

  // Replace in tree
  _tree.replace(entry->node_addr(), cursor);

  // And in size class lists

  // Remove old
  const size_t old_size = old_entry->vmem().size();
  const int old_index = size_class_index(old_size);
  if (old_index != -1) {
    _size_class_lists[old_index].remove(old_entry->size_class_node());
  }

  // Insert new
  const size_t new_size = vmem.size();
  const int new_index = size_class_index(new_size);
  if (new_index != -1) {
    _size_class_lists[new_index].insert_first(entry->size_class_node());
  }

  // Destroy old entry
  old_entry->~ZMappedCacheEntry();
}

void ZMappedCache::tree_update(ZMappedCacheEntry* entry, const ZVirtualMemory& vmem) {
  assert(entry->end() == vmem.end(), "must be");

  // Remove or add to lists if required
  const size_t old_size = entry->vmem().size();
  const size_t new_size = vmem.size();
  const int old_index = size_class_index(old_size);
  const int new_index = size_class_index(new_size);
  if (old_index != new_index) {
    // Size class changed

    // Remove old
    if (old_index != -1) {
      _size_class_lists[old_index].remove(entry->size_class_node());
    }

    // Insert new
    if (new_index != -1) {
      _size_class_lists[new_index].insert_first(entry->size_class_node());
    }
  }

  // And update entry
  entry->update_start(vmem.start());
}

template <ZMappedCache::RemovalStrategy strategy, typename SelectFunction>
ZVirtualMemory ZMappedCache::remove_vmem(ZMappedCacheEntry* const entry, size_t min_size, SelectFunction select) {
  ZVirtualMemory vmem = entry->vmem();
  const size_t size = vmem.size();

  if (size < min_size) {
    // Do not select this, smaller than min_size
    return ZVirtualMemory();
  }

  // Query how much to remove
  const size_t to_remove = select(size);
  assert(to_remove <= size, "must not remove more than size");

  if (to_remove == 0) {
    // Nothing to remove
    return ZVirtualMemory();
  }

  if (to_remove != size) {
    // Partial removal
    if (strategy == RemovalStrategy::LowestAddress) {
      const size_t unused_size = size - to_remove;
      const ZVirtualMemory unused_vmem = vmem.split_from_back(unused_size);
      tree_update(entry, unused_vmem);
    } else {
      assert(strategy == RemovalStrategy::HighestAddress, "must be LowestAddress or HighestAddress");

      const size_t unused_size = size - to_remove;
      const ZVirtualMemory unused_vmem = vmem.split_from_front(unused_size);

      auto cursor = _tree.get_cursor(entry->node_addr());
      assert(cursor.is_valid(), "must be");
      tree_replace(cursor, unused_vmem);
    }
  } else {
    // Whole removal
    auto cursor = _tree.get_cursor(entry->node_addr());
    assert(cursor.is_valid(), "must be");
    tree_remove(cursor, vmem);
  }

  // Update statistics
  _size -= to_remove;
  _min = MIN2(_size, _min);

  postcond(to_remove == vmem.size());
  return vmem;
}

template <typename MaxSelectFunction, typename SelectFunction, typename ConsumeFunction>
bool ZMappedCache::try_remove_vmem_size_class(size_t min_size, MaxSelectFunction max_select, SelectFunction select, ConsumeFunction consume) {
  // Start scanning lists using the max remaining size
  for (size_t last_max_size = 0, max_size = max_select();
       max_size != last_max_size;
       last_max_size = max_size, max_size = max_select()) {
    assert(min_size <= max_size, "must be %zu <= %zu", min_size, max_size);
    // Start scaning from max_size guaranteed size class to the largest size class
    const int guaranteed_index = guaranteed_size_class_index(max_size);
    for (int index = guaranteed_index; index != -1 && index < NumSizeClasses; ++index) {
      ZList<ZSizeClassListNode>& list = _size_class_lists[index];
      if (!list.is_empty()) {
        // Because this is guaranteed, select should always succeed
        ZMappedCacheEntry* const entry = ZMappedCacheEntry::cast_to_entry(list.first());
        const ZVirtualMemory vmem = remove_vmem<RemovalStrategy::LowestAddress>(entry, min_size, select);
        assert(!vmem.is_null(), "select must succeed");
        if (consume(vmem)) {
          // consume  is satisfied
          return true;
        }

        // Continue with new max remaining size
        break;
      }
    }
  }

  // Consume the rest starting at max_size's size class to min_size's size class
  const size_t max_size = max_select();
  const int max_size_index = size_class_index(max_size);
  const int min_size_index = size_class_index(min_size);
  const int lowest_index = MAX2(min_size_index, 0);

  for (int index = max_size_index; index >= lowest_index; --index) {
    ZListIterator<ZSizeClassListNode> iter(&_size_class_lists[index]);
    for (ZSizeClassListNode* list_node; iter.next(&list_node);) {
      ZMappedCacheEntry* const entry = ZMappedCacheEntry::cast_to_entry(list_node);
      const ZVirtualMemory vmem = remove_vmem<RemovalStrategy::LowestAddress>(entry, min_size, select);
      if (!vmem.is_null() && consume(vmem)) {
        // Found a vmem and consume is satisfied
        return true;
      }
    }
  }

  // consumed was not satisfied
  return false;
}

template <ZMappedCache::RemovalStrategy strategy, typename MaxSelectFunction, typename SelectFunction, typename ConsumeFunction>
void ZMappedCache::scan_remove_vmem(size_t min_size, MaxSelectFunction max_select, SelectFunction select, ConsumeFunction consume) {
  if (strategy == RemovalStrategy::SizeClasses) {
    if (try_remove_vmem_size_class(min_size, max_select, select, consume)) {
      // Satisfied using size classes
      return;
    } else if (size_class_index(min_size) != -1) {
      // There exists a size class for our min size. All possibilities must have
      // been exhausted, do not scan the tree.
      return;
    }

    // Fallthrough to tree scan
  }

  if (strategy == RemovalStrategy::HighestAddress) {
    // Scan whole tree starting at the highest address
    for (ZMappedCache::TreeNode* node = _tree.last(); node != nullptr; node = node->prev()) {
      ZMappedCacheEntry* const entry = ZMappedCacheEntry::cast_to_entry(node);
      const ZVirtualMemory vmem = remove_vmem<RemovalStrategy::HighestAddress>(entry, min_size, select);
      if (!vmem.is_null() && consume(vmem)) {
        // Found a vmem and consume is satisfied.
        return;
      }
    }
  } else {
    assert(strategy == RemovalStrategy::SizeClasses || strategy == RemovalStrategy::LowestAddress, "unknown strategy");

    // Scan whole tree starting at the lowest address
    for (ZMappedCache::TreeNode* node = _tree.first(); node != nullptr; node = node->next()) {
      ZMappedCacheEntry* const entry = ZMappedCacheEntry::cast_to_entry(node);
      const ZVirtualMemory vmem = remove_vmem<RemovalStrategy::LowestAddress>(entry, min_size, select);
      if (!vmem.is_null() && consume(vmem)) {
        // Found a vmem and consume is satisfied.
        return;
      }
    }
  }

}

template <ZMappedCache::RemovalStrategy strategy, typename MaxSelectFunction, typename SelectFunction, typename ConsumeFunction>
void ZMappedCache::scan_remove_vmem(MaxSelectFunction max_select, SelectFunction select, ConsumeFunction consume) {
  // Scan without a min_size
  scan_remove_vmem<strategy>(0, max_select, select, consume);
}

template <ZMappedCache::RemovalStrategy strategy>
size_t ZMappedCache::remove_discontiguous_with_strategy(ZArray<ZVirtualMemory>* vmems, size_t size) {
  precond(size > 0);
  precond(size % ZGranuleSize == 0);

  size_t remaining = size;
  const auto max_select = [&]() {
    // Select at most remaining
    return remaining;
  };

  const auto select_vmem = [&](size_t vmem_size) {
    // Select at most remaining
    return MIN2(remaining, vmem_size);
  };

  const auto consume_vmem = [&](ZVirtualMemory vmem) {
    const size_t vmem_size = vmem.size();
    vmems->append(vmem);

    assert(vmem_size <= remaining, "consumed to much");

    // Track remaining, and stop when it reaches zero
    remaining -= vmem_size;
    return remaining == 0;
  };

  scan_remove_vmem<strategy>(max_select, select_vmem, consume_vmem);

  return size - remaining;
}

ZMappedCache::ZMappedCache()
  : _tree(),
    _size_class_lists{},
    _size(0),
    _min(_size) {}

void ZMappedCache::insert(const ZVirtualMemory& vmem) {
  _size += vmem.size();

  Tree::FindCursor current_cursor = _tree.find(vmem.start());
  Tree::FindCursor next_cursor = _tree.next(current_cursor);

  const bool extends_left = current_cursor.found();
  const bool extends_right = next_cursor.is_valid() && next_cursor.found() &&
                             ZMappedCacheEntry::cast_to_entry(next_cursor.node())->start() == vmem.end();

  if (extends_left && extends_right) {
    ZMappedCacheEntry* next_entry = ZMappedCacheEntry::cast_to_entry(next_cursor.node());

    const ZVirtualMemory left_vmem = ZMappedCacheEntry::cast_to_entry(current_cursor.node())->vmem();
    const ZVirtualMemory right_vmem = next_entry->vmem();
    assert(left_vmem.adjacent_to(vmem), "must be");
    assert(vmem.adjacent_to(right_vmem), "must be");

    ZVirtualMemory new_vmem = left_vmem;
    new_vmem.grow_from_back(vmem.size());
    new_vmem.grow_from_back(right_vmem.size());

    // Remove current (left vmem)
    tree_remove(current_cursor, left_vmem);

    // And update next's start
    tree_update(next_entry, new_vmem);

    return;
  }

  if (extends_left) {
    const ZVirtualMemory left_vmem = ZMappedCacheEntry::cast_to_entry(current_cursor.node())->vmem();
    assert(left_vmem.adjacent_to(vmem), "must be");

    ZVirtualMemory new_vmem = left_vmem;
    new_vmem.grow_from_back(vmem.size());

    tree_replace(current_cursor, new_vmem);

    return;
  }

  if (extends_right) {
    ZMappedCacheEntry* next_entry = ZMappedCacheEntry::cast_to_entry(next_cursor.node());

    const ZVirtualMemory right_vmem = next_entry->vmem();
    assert(vmem.adjacent_to(right_vmem), "must be");

    ZVirtualMemory new_vmem = vmem;
    new_vmem.grow_from_back(right_vmem.size());

    // Update next's start
    tree_update(next_entry, new_vmem);

    return;
  }

  tree_insert(current_cursor, vmem);
}

ZVirtualMemory ZMappedCache::remove_contiguous(size_t size) {
  precond(size > 0);
  precond(size % ZGranuleSize == 0);

  ZVirtualMemory result;

  const auto max_select = [&]() {
    // We always select the size
    return size;
  };

  const auto select_vmem = [&](size_t) {
    // We always select the size
    return size;
  };

  const auto consume_vmem = [&](ZVirtualMemory vmem) {
    assert(result.is_null(), "only consume once");
    assert(vmem.size() == size, "wrong size consumed");

    result = vmem;

    // Only require one vmem
    return true;
  };

  if (size == ZPageSizeSmall) {
    // For small page allocation allocate at the lowest address
    scan_remove_vmem<RemovalStrategy::LowestAddress>(size, max_select, select_vmem, consume_vmem);
  } else {
    // Other sizes uses approximate best fit size classes first
    scan_remove_vmem<RemovalStrategy::SizeClasses>(size, max_select, select_vmem, consume_vmem);
  }

  return result;
}

size_t ZMappedCache::remove_discontiguous(ZArray<ZVirtualMemory>* vmems, size_t size) {
  return remove_discontiguous_with_strategy<RemovalStrategy::SizeClasses>(vmems, size);
}

size_t ZMappedCache::reset_min() {
  const size_t old_min = _min;
  _min = _size;

  return old_min;
}

size_t ZMappedCache::remove_from_min(ZArray<ZVirtualMemory>* vmems, size_t max_size) {
  const size_t size = MIN2(_min, max_size);
  if (size == 0) {
    return 0;
  }

  return remove_discontiguous_with_strategy<RemovalStrategy::HighestAddress>(vmems, size);
}

size_t ZMappedCache::size() const {
  return _size;
}
