/*
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
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

#include "gc/shared/gc_globals.hpp"
#include "gc/shenandoah/jvmFlagConstraintsShenandoah.hpp"
#include "runtime/globals.hpp"
#include "utilities/globalDefinitions.hpp"

JVMFlag::Error ShenandoahMaxRegionSizeConstraintFunc(size_t value, bool verbose) {
  // Protect for overflows in conservative_max_heap_alignment
  if (value >= max_power_of_2<size_t>()) {
    JVMFlag::printError(verbose,
        "ShenandoahMaxRegionSize %zu%s should be lower than (%zu%s).\n",
        byte_size_in_proper_unit(value), proper_unit_for_byte_size(value),
        byte_size_in_proper_unit(max_power_of_2<size_t>()),proper_unit_for_byte_size(max_power_of_2<size_t>()));
    return JVMFlag::VIOLATES_CONSTRAINT;
  } else {
    return JVMFlag::SUCCESS;
  }
}
