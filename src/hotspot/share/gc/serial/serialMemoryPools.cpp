/*
 * Copyright (c) 2017, 2024, Oracle and/or its affiliates. All rights reserved.
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
#include "gc/serial/defNewGeneration.hpp"
#include "gc/serial/serialMemoryPools.hpp"
#include "gc/serial/tenuredGeneration.hpp"
#include "gc/shared/space.hpp"

ContiguousSpacePool::ContiguousSpacePool(ContiguousSpace* space,
                                         const char* name,
                                         Bytes max_size,
                                         bool support_usage_threshold) :
  CollectedMemoryPool(name, space->capacity(), max_size,
                      support_usage_threshold), _space(space) {
}

Bytes ContiguousSpacePool::used_in_bytes() {
  return space()->used();
}

MemoryUsage ContiguousSpacePool::get_memory_usage() {
  Bytes maxSize   = (available_for_allocation() ? max_size() : Bytes(0));
  Bytes used      = used_in_bytes();
  Bytes committed = _space->capacity();

  return MemoryUsage(initial_size(), used, committed, maxSize);
}

SurvivorContiguousSpacePool::SurvivorContiguousSpacePool(DefNewGeneration* young_gen,
                                                         const char* name,
                                                         Bytes max_size,
                                                         bool support_usage_threshold) :
  CollectedMemoryPool(name, young_gen->from()->capacity(), max_size,
                      support_usage_threshold), _young_gen(young_gen) {
}

Bytes SurvivorContiguousSpacePool::used_in_bytes() {
  return _young_gen->from()->used();
}

Bytes SurvivorContiguousSpacePool::committed_in_bytes() {
  return _young_gen->from()->capacity();
}

MemoryUsage SurvivorContiguousSpacePool::get_memory_usage() {
  Bytes maxSize = (available_for_allocation() ? max_size() : Bytes(0));
  Bytes used    = used_in_bytes();
  Bytes committed = committed_in_bytes();

  return MemoryUsage(initial_size(), used, committed, maxSize);
}

TenuredGenerationPool::TenuredGenerationPool(TenuredGeneration* gen,
                                             const char* name,
                                             bool support_usage_threshold) :
  CollectedMemoryPool(name, gen->capacity(), gen->max_capacity(),
                      support_usage_threshold), _gen(gen) {
}

Bytes TenuredGenerationPool::used_in_bytes() {
  return _gen->used();
}

MemoryUsage TenuredGenerationPool::get_memory_usage() {
  Bytes used      = used_in_bytes();
  Bytes committed = _gen->capacity();
  Bytes maxSize   = (available_for_allocation() ? max_size() : Bytes(0));

  return MemoryUsage(initial_size(), used, committed, maxSize);
}
