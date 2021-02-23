/*
 * Copyright (c) 2017, 2022, Oracle and/or its affiliates. All rights reserved.
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
#include "gc/g1/g1FullGCCompactionPoint.hpp"
#include "gc/g1/heapRegion.hpp"
#include "gc/g1/heapRegionVector.hpp"
#include "oops/oop.inline.hpp"
#include "utilities/debug.hpp"

G1FullGCCompactionPoint::G1FullGCCompactionPoint() :
    _current_region(NULL),
    _compaction_top(NULL),
    _compaction_regions(),
    _compaction_region_index(0)
{
  _compaction_regions.reserve(32);
}

G1FullGCCompactionPoint::~G1FullGCCompactionPoint() {}

void G1FullGCCompactionPoint::update() {
  if (is_initialized()) {
    _current_region->set_compaction_top(_compaction_top);
  }
}

void G1FullGCCompactionPoint::initialize_values() {
  _compaction_top = _current_region->compaction_top();
}

bool G1FullGCCompactionPoint::has_regions() {
  return !_compaction_regions.empty();
}

bool G1FullGCCompactionPoint::is_initialized() {
  return _current_region != NULL;
}

void G1FullGCCompactionPoint::initialize(HeapRegion* hr) {
  _current_region = hr;
  initialize_values();
}

HeapRegion* G1FullGCCompactionPoint::current_region() {
  assert(_compaction_region_index < _compaction_regions.size(), "precondition");
  HeapRegion* region = _compaction_regions[_compaction_region_index];
  assert(region != NULL, "Must return valid region");
  return region;
}

HeapRegion* G1FullGCCompactionPoint::next_region() {
  assert(_compaction_region_index < _compaction_regions.size(), "precondition");
  ++_compaction_region_index;
  assert(_compaction_region_index < _compaction_regions.size(), "no more regions");
  HeapRegion* next = _compaction_regions[_compaction_region_index];
  assert(next != NULL, "Must return valid region");
  return next;
}

HeapRegionVector* G1FullGCCompactionPoint::regions() {
  return &_compaction_regions;
}

bool G1FullGCCompactionPoint::object_will_fit(size_t size) {
  size_t space_left = pointer_delta(_current_region->end(), _compaction_top);
  return size <= space_left;
}

void G1FullGCCompactionPoint::switch_region() {
  // Save compaction top in the region.
  _current_region->set_compaction_top(_compaction_top);
  // Get the next region and re-initialize the values.
  _current_region = next_region();
  initialize_values();
}

void G1FullGCCompactionPoint::forward(oop object, size_t size) {
  assert(_current_region != NULL, "Must have been initialized");

  // Ensure the object fit in the current region.
  while (!object_will_fit(size)) {
    switch_region();
  }

  // Store a forwarding pointer if the object should be moved.
  if (cast_from_oop<HeapWord*>(object) != _compaction_top) {
    object->forward_to(cast_to_oop(_compaction_top));
    assert(object->is_forwarded(), "must be forwarded");
  } else {
    assert(!object->is_forwarded(), "must not be forwarded");
  }

  // Update compaction values.
  _compaction_top += size;
  _current_region->update_bot_for_block(_compaction_top - size, _compaction_top);
}

void G1FullGCCompactionPoint::add(HeapRegion* hr) {
  _compaction_regions.push_back(hr);
}

HeapRegion* G1FullGCCompactionPoint::remove_last() {
  HeapRegion* result = NULL;
  if (!_compaction_regions.empty()) {
    result = _compaction_regions.back();
    _compaction_regions.pop_back();
  }
  return result;
}
