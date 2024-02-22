/*
 * Copyright (c) 2001, 2023, Oracle and/or its affiliates. All rights reserved.
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
#include "gc/shared/collectedHeap.hpp"
#include "gc/shared/gc_globals.hpp"
#include "gc/shared/plab.inline.hpp"
#include "gc/shared/threadLocalAllocBuffer.hpp"
#include "gc/shared/tlab_globals.hpp"
#include "logging/log.hpp"
#include "memory/universe.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/globals_extension.hpp"

Words PLAB::min_size() {
  // Make sure that we return something that is larger than AlignmentReserve
  return align_object_size(MAX2(to_Words(in_Bytes(MinTLABSize)), oopDesc::header_size())) + CollectedHeap::lab_alignment_reserve();
}

Words PLAB::max_size() {
  return ThreadLocalAllocBuffer::max_size();
}

void PLAB::startup_initialization() {
  if (!FLAG_IS_DEFAULT(MinTLABSize)) {
    if (FLAG_IS_DEFAULT(YoungPLABSize)) {
      FLAG_SET_ERGO(YoungPLABSize, untype(MAX2(ThreadLocalAllocBuffer::min_size(), in_Words(YoungPLABSize))));
    }
    if (FLAG_IS_DEFAULT(OldPLABSize)) {
      FLAG_SET_ERGO(OldPLABSize, untype(MAX2(ThreadLocalAllocBuffer::min_size(), in_Words(OldPLABSize))));
    }
  }
  if (!is_object_aligned(in_Words(YoungPLABSize))) {
    FLAG_SET_ERGO(YoungPLABSize, untype(align_object_size(in_Words(YoungPLABSize))));
  }
  if (!is_object_aligned(in_Words(OldPLABSize))) {
    FLAG_SET_ERGO(OldPLABSize, untype(align_object_size(in_Words(OldPLABSize))));
  }
}

PLAB::PLAB(Words desired_plab_sz_) :
  _word_sz(desired_plab_sz_), _bottom(nullptr), _top(nullptr),
  _end(nullptr), _hard_end(nullptr), _allocated(Words(0)), _wasted(Words(0)), _undo_wasted(Words(0))
{
  assert(min_size() > CollectedHeap::lab_alignment_reserve(),
         "Minimum PLAB size " SIZE_FORMAT " must be larger than alignment reserve " SIZE_FORMAT " "
         "to be able to contain objects", min_size(), CollectedHeap::lab_alignment_reserve());
}

void PLAB::flush_and_retire_stats(PLABStats* stats) {
  // Retire the last allocation buffer.
  Words unused = retire_internal();

  // Now flush the statistics.
  stats->add_allocated(_allocated);
  stats->add_wasted(_wasted);
  stats->add_undo_wasted(_undo_wasted);
  stats->add_unused(unused);

  // Since we have flushed the stats we need to clear  the _allocated and _wasted
  // fields in case somebody retains an instance of this over GCs. Not doing so
  // will artificially inflate the values in the statistics.
  _allocated   = Words(0);
  _wasted      = Words(0);
  _undo_wasted = Words(0);
}

void PLAB::retire() {
  _wasted += retire_internal();
}

Words PLAB::retire_internal() {
  Words result = Words(0);
  if (_top < _hard_end) {
    Universe::heap()->fill_with_dummy_object(_top, _hard_end, true);
    result += invalidate();
  }
  return result;
}

void PLAB::add_undo_waste(HeapWord* obj, Words word_sz) {
  Universe::heap()->fill_with_dummy_object(obj, obj + word_sz, true);
  _undo_wasted += word_sz;
}

void PLAB::undo_last_allocation(HeapWord* obj, Words word_sz) {
  assert(pointer_delta(_top, _bottom) >= word_sz, "Bad undo");
  assert(pointer_delta(_top, obj) == word_sz, "Bad undo");
  _top = obj;
}

void PLAB::undo_allocation(HeapWord* obj, Words word_sz) {
  // Is the alloc in the current alloc buffer?
  if (contains(obj)) {
    assert(contains(obj + word_sz - 1),
      "should contain whole object");
    undo_last_allocation(obj, word_sz);
  } else {
    add_undo_waste(obj, word_sz);
  }
}
