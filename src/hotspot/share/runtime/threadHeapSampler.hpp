/*
 * Copyright (c) 2018, 2020, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2018, Google and/or its affiliates. All rights reserved.
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

#ifndef SHARE_RUNTIME_THREADHEAPSAMPLER_HPP
#define SHARE_RUNTIME_THREADHEAPSAMPLER_HPP

#include "memory/allocation.hpp"
#include "oops/oopsHierarchy.hpp"

class ThreadHeapSampler {
 private:
  // Amount of bytes to allocate before taking the next sample
  size_t _sample_threshold;

  // The TLAB top address when the last sampling happened, or
  // TLAB start if a new TLAB is allocated
  HeapWord* _tlab_sample_start;

  // The accumulated amount of allocated bytes in a TLAB since the last sampling
  // excluding the amount between _tlab_sample_start and top
  size_t _tlab_bytes;

  // The accumulated amount of allocated bytes outside TLABs since last sample point
  size_t _outside_tlab_bytes;

  // Cheap random number generator
  static uint64_t _rnd;

  static volatile int _sampling_interval;

  void pick_next_geometric_sample();
  void pick_next_sample();

  static double fast_log2(const double& d);
  uint64_t next_random(uint64_t rnd);

 public:
  ThreadHeapSampler() :
      _tlab_sample_start(nullptr),
      _tlab_bytes(0),
      _outside_tlab_bytes(0) {
    _rnd = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this));
    if (_rnd == 0) {
      _rnd = 1;
    }

    // Call this after _rnd is initialized to initialize _bytes_until_sample.
    pick_next_sample();
  }

  size_t sample_threshold() const {
    return _sample_threshold;
  }

  void set_tlab_sample_start(HeapWord* ptr) {
    _tlab_sample_start = ptr;
  }

  void reset_after_sampling(HeapWord* tlab_top) {
    _tlab_sample_start = tlab_top;
    _tlab_bytes = 0;
    _outside_tlab_bytes = 0;
  }

  size_t tlab_unsampled(HeapWord* tlab_top)  const {
    return pointer_delta(tlab_top, _tlab_sample_start, 1);
  }

  size_t tlab_bytes_since_sample(HeapWord* tlab_top) const {
    return _tlab_bytes + tlab_unsampled(tlab_top);
  }

  void accumulate_tlab_unsampled(HeapWord* tlab_top) {
    _tlab_bytes += tlab_unsampled(tlab_top);
  }

  void inc_outside_tlab_bytes(size_t size) {
    _outside_tlab_bytes += size;
  }

  size_t outside_tlab_bytes() const {
    return _outside_tlab_bytes;
  }

  void report_sample(const char* message, size_t unaccounted_tlab_bytes);

  void sample(oop obj, HeapWord* tlab_top);

  static void set_sampling_interval(int sampling_interval);
  static int get_sampling_interval();
};

#endif // SHARE_RUNTIME_THREADHEAPSAMPLER_HPP
