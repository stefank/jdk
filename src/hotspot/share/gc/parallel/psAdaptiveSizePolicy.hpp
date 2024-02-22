/*
 * Copyright (c) 2002, 2023, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_GC_PARALLEL_PSADAPTIVESIZEPOLICY_HPP
#define SHARE_GC_PARALLEL_PSADAPTIVESIZEPOLICY_HPP

#include "gc/shared/adaptiveSizePolicy.hpp"
#include "gc/shared/gcCause.hpp"
#include "gc/shared/gcStats.hpp"
#include "gc/shared/gcUtil.hpp"
#include "utilities/align.hpp"

// This class keeps statistical information and computes the
// optimal free space for both the young and old generation
// based on current application characteristics (based on gc cost
// and application footprint).
//
// It also computes an optimal tenuring threshold between the young
// and old generations, so as to equalize the cost of collections
// of those generations, as well as optimal survivor space sizes
// for the young generation.
//
// While this class is specifically intended for a generational system
// consisting of a young gen (containing an Eden and two semi-spaces)
// and a tenured gen, as well as a perm gen for reflective data, it
// makes NO references to specific generations.
//
// 05/02/2003 Update
// The 1.5 policy makes use of data gathered for the costs of GC on
// specific generations.  That data does reference specific
// generation.  Also diagnostics specific to generations have
// been added.

// Forward decls
class elapsedTimer;

class PSAdaptiveSizePolicy : public AdaptiveSizePolicy {
 friend class PSGCAdaptivePolicyCounters;
 private:
  // These values are used to record decisions made during the
  // policy.  For example, if the young generation was decreased
  // to decrease the GC cost of minor collections the value
  // decrease_young_gen_for_throughput_true is used.

  // Last calculated sizes, in bytes, and aligned
  // NEEDS_CLEANUP should use sizes.hpp,  but it works in ints, not size_t's

  // Time statistics
  AdaptivePaddedAverage* _avg_major_pause;

  // Footprint statistics
  AdaptiveWeightedAverage* _avg_base_footprint;

  // Statistical data gathered for GC
  GCStats _gc_stats;

  // Variable for estimating the major and minor pause times.
  // These variables represent linear least-squares fits of
  // the data.
  //   major pause time vs. old gen size
  LinearLeastSquareFit* _major_pause_old_estimator;
  //   major pause time vs. young gen size
  LinearLeastSquareFit* _major_pause_young_estimator;


  // These record the most recent collection times.  They
  // are available as an alternative to using the averages
  // for making ergonomic decisions.
  double _latest_major_mutator_interval_seconds;

  const Bytes _space_alignment; // alignment for eden, survivors

  // The amount of live data in the heap at the last full GC, used
  // as a baseline to help us determine when we need to perform the
  // next full GC.
  Bytes _live_at_last_full_gc;

  // decrease/increase the old generation for minor pause time
  int _change_old_gen_for_min_pauses;

  // increase/decrease the young generation for major pause time
  int _change_young_gen_for_maj_pauses;

  // To facilitate faster growth at start up, supplement the normal
  // growth percentage for the young gen eden and the
  // old gen space for promotion with these value which decay
  // with increasing collections.
  uint _young_gen_size_increment_supplement;
  uint _old_gen_size_increment_supplement;

 private:

  void adjust_eden_for_minor_pause_time(Bytes* desired_eden_size_ptr);
  // Change the generation sizes to achieve a GC pause time goal
  // Returned sizes are not necessarily aligned.
  void adjust_promo_for_pause_time(Bytes* desired_promo_size_ptr);
  void adjust_eden_for_pause_time(Bytes* desired_eden_size_ptr);
  // Change the generation sizes to achieve an application throughput goal
  // Returned sizes are not necessarily aligned.
  void adjust_promo_for_throughput(bool is_full_gc,
                             Bytes* desired_promo_size_ptr);
  void adjust_eden_for_throughput(bool is_full_gc,
                             Bytes* desired_eden_size_ptr);
  // Change the generation sizes to achieve minimum footprint
  // Returned sizes are not aligned.
  Bytes adjust_promo_for_footprint(Bytes desired_promo_size,
                                   Bytes desired_total);
  Bytes adjust_eden_for_footprint(Bytes desired_promo_size,
                                  Bytes desired_total);

  // Size in bytes for an increment or decrement of eden.
  Bytes eden_decrement_aligned_down(Bytes cur_eden);
  Bytes eden_increment_with_supplement_aligned_up(Bytes cur_eden);

  // Size in bytes for an increment or decrement of the promotion area
  Bytes promo_decrement_aligned_down(Bytes cur_promo);
  Bytes promo_increment_with_supplement_aligned_up(Bytes cur_promo);

  // Returns a change that has been scaled down.  Result
  // is not aligned.  (If useful, move to some shared
  // location.)
  size_t scale_down(size_t change, double part, double total);

 protected:

  // Footprint accessors
  size_t live_space() const {
    return (size_t)(avg_base_footprint()->average() +
                    avg_young_live()->average() +
                    avg_old_live()->average());
  }
  Bytes free_space() const {
    return _eden_size + _promo_size;
  }

  void set_promo_size(Bytes new_size) {
    _promo_size = new_size;
  }

  // Update estimators
  void update_minor_pause_old_estimator(double minor_pause_in_ms);

  virtual GCPolicyKind kind() const { return _gc_ps_adaptive_size_policy; }

 public:
  // Accessors for use by performance counters
  AdaptivePaddedNoZeroDevAverage*  avg_promoted() const {
    return _gc_stats.avg_promoted();
  }
  AdaptiveWeightedAverage* avg_base_footprint() const {
    return _avg_base_footprint;
  }

  // Input arguments are initial free space sizes for young and old
  // generations, the initial survivor space size, the
  // alignment values and the pause & throughput goals.
  //
  // NEEDS_CLEANUP this is a singleton object
  PSAdaptiveSizePolicy(Bytes init_eden_size,
                       Bytes init_promo_size,
                       Bytes init_survivor_size,
                       Bytes space_alignment,
                       double gc_pause_goal_sec,
                       uint gc_time_ratio);

  // Methods indicating events of interest to the adaptive size policy,
  // called by GC algorithms. It is the responsibility of users of this
  // policy to call these methods at the correct times!
  void major_collection_begin();
  void major_collection_end(Bytes amount_live, GCCause::Cause gc_cause);

  void tenured_allocation(Bytes size) {
    _avg_pretenured->sample(untype(size));
  }

  // Accessors
  // NEEDS_CLEANUP   should use sizes.hpp !!!!!!!!!

  static Bytes calculate_free_based_on_live(Bytes live, uintx ratio_as_percentage);

  Bytes calculated_old_free_size_in_bytes() const;

  Bytes average_promoted_in_bytes() const {
    return in_Bytes((size_t)avg_promoted()->average());
  }

  Bytes padded_average_promoted_in_bytes() const {
    return in_Bytes((size_t)avg_promoted()->padded_average());
  }

  int change_young_gen_for_maj_pauses() {
    return _change_young_gen_for_maj_pauses;
  }
  void set_change_young_gen_for_maj_pauses(int v) {
    _change_young_gen_for_maj_pauses = v;
  }

  int change_old_gen_for_min_pauses() {
    return _change_old_gen_for_min_pauses;
  }
  void set_change_old_gen_for_min_pauses(int v) {
    _change_old_gen_for_min_pauses = v;
  }

  // Accessors for estimators.  The slope of the linear fit is
  // currently all that is used for making decisions.

  LinearLeastSquareFit* major_pause_old_estimator() {
    return _major_pause_old_estimator;
  }

  virtual void clear_generation_free_space_flags();

  double major_pause_old_slope() { return _major_pause_old_estimator->slope(); }
  double major_pause_young_slope() {
    return _major_pause_young_estimator->slope();
  }

  // Calculates optimal (free) space sizes for both the young and old
  // generations.  Stores results in _eden_size and _promo_size.
  // Takes current used space in all generations as input, as well
  // as an indication if a full gc has just been performed, for use
  // in deciding if an OOM error should be thrown.
  void compute_generations_free_space(Bytes young_live,
                                      Bytes eden_live,
                                      Bytes old_live,
                                      Bytes cur_eden,  // current eden in bytes
                                      Bytes max_old_gen_size,
                                      Bytes max_eden_size,
                                      bool   is_full_gc);

  void compute_eden_space_size(Bytes young_live,
                               Bytes eden_live,
                               Bytes cur_eden,  // current eden in bytes
                               Bytes max_eden_size,
                               bool   is_full_gc);

  void compute_old_gen_free_space(Bytes old_live,
                                  Bytes cur_eden,  // current eden in bytes
                                  Bytes max_old_gen_size,
                                  bool   is_full_gc);

  // Calculates new survivor space size;  returns a new tenuring threshold
  // value. Stores new survivor size in _survivor_size.
  uint compute_survivor_space_size_and_threshold(bool   is_survivor_overflow,
                                                 uint    tenuring_threshold,
                                                 Bytes survivor_limit);

  // Return the maximum size of a survivor space if the young generation were of
  // size gen_size.
  Bytes max_survivor_size(Bytes gen_size) {
    // Never allow the target survivor size to grow more than MinSurvivorRatio
    // of the young generation size.  We cannot grow into a two semi-space
    // system, with Eden zero sized.  Even if the survivor space grows, from()
    // might grow by moving the bottom boundary "down" -- so from space will
    // remain almost full anyway (top() will be near end(), but there will be a
    // large filler object at the bottom).
    const Bytes sz = gen_size / MinSurvivorRatio;
    const Bytes alignment = _space_alignment;
    return sz > alignment ? align_down(sz, alignment) : alignment;
  }

  Bytes live_at_last_full_gc() {
    return _live_at_last_full_gc;
  }

  // Update averages that are always used (even
  // if adaptive sizing is turned off).
  void update_averages(bool is_survivor_overflow,
                       Bytes survived,
                       Bytes promoted);

  // Printing support
  virtual bool print() const;

  // Decay the supplemental growth additive.
  void decay_supplemental_growth(bool is_full_gc);
};

#endif // SHARE_GC_PARALLEL_PSADAPTIVESIZEPOLICY_HPP
