/*
 * Copyright (c) 2020, 2022, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2020 SAP SE. All rights reserved.
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

#ifndef SHARE_MEMORY_METASPACE_TESTHELPERS_HPP
#define SHARE_MEMORY_METASPACE_TESTHELPERS_HPP

#include "memory/allocation.hpp"
#include "memory/metaspace.hpp"
#include "memory/metaspace/commitLimiter.hpp"
#include "memory/metaspace/counters.hpp"
#include "memory/metaspace/metaspaceContext.hpp"
#include "memory/virtualspace.hpp"
#include "utilities/globalDefinitions.hpp"

// This is just convenience classes for metaspace-related tests
//  (jtreg, via whitebox API, and gtests)

class ReservedSpace;
class Mutex;
class outputStream;

namespace metaspace {

class MetaspaceContext;
class MetaspaceArena;

// Wraps a MetaspaceTestArena with its own lock for testing purposes.
class MetaspaceTestArena : public CHeapObj<mtInternal> {

  Mutex* const _lock;
  MetaspaceArena* const _arena;

public:

  const MetaspaceArena* arena() const {
    return _arena;
  }

  MetaspaceTestArena(Mutex* lock, MetaspaceArena* arena);
  ~MetaspaceTestArena();

  MetaWord* allocate(Words word_size);
  void deallocate(MetaWord* p, Words word_size);

};

// Wraps an instance of a MetaspaceContext together with some side objects for easy use in test beds (whitebox, gtests)
class MetaspaceTestContext : public CHeapObj<mtInternal> {

  const char* const _name;
  const Words _reserve_limit;
  const Words _commit_limit;

  MetaspaceContext* _context;
  CommitLimiter _commit_limiter;
  WordsAtomicCounter _used_words_counter;

  // For non-expandable contexts we keep track of the space
  // and delete it at destruction time.
  ReservedSpace _rs;

public:

  // Note: limit == 0 means unlimited
  // Reserve limit > 0 simulates a non-expandable VirtualSpaceList (like CompressedClassSpace)
  // Commit limit > 0 simulates a limit to max committable space (like MaxMetaspaceSize)
  MetaspaceTestContext(const char* name, Words commit_limit = Words(0), Words reserve_limit = Words(0));
  ~MetaspaceTestContext();

  // Create an arena, feeding off this area.
  MetaspaceTestArena* create_arena(Metaspace::MetaspaceType type);

  void purge_area();

  // Accessors
  const CommitLimiter& commit_limiter() const { return _commit_limiter; }
  const VirtualSpaceList& vslist() const      { return *(_context->vslist()); }
  ChunkManager& cm()                          { return *(_context->cm()); }

  // Returns reserve- and commit limit we run the test with (in the real world,
  // these would be equivalent to CompressedClassSpaceSize resp MaxMetaspaceSize)
  // FIXME: This looks like a bug. Why not return _reserve_limit?
  Words reserve_limit() const    { return in_Words(_reserve_limit == Words(0) ? max_uintx : 0); }
  Words commit_limit() const     { return in_Words(_commit_limit == Words(0) ? max_uintx : 0); }

  // Convenience function to retrieve total committed/used words
  Words used_words() const       { return _used_words_counter.get(); }
  Words committed_words() const  { return _commit_limiter.committed_words(); }

  DEBUG_ONLY(void verify() const;)

  void print_on(outputStream* st) const;

};

} // namespace metaspace

#endif // SHARE_MEMORY_METASPACE_TESTHELPERS_HPP
