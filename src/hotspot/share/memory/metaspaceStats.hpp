/*
 * Copyright (c) 2021, 2022, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2021 SAP SE. All rights reserved.
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
#ifndef SHARE_MEMORY_METASPACESTATS_HPP
#define SHARE_MEMORY_METASPACESTATS_HPP

#include "utilities/globalDefinitions.hpp"

// Data holder classes for metaspace statistics.
//
// - MetaspaceStats: keeps reserved, committed and used byte counters;
//                   retrieve with MetaspaceUtils::get_statistics(MetadataType) for either class space
//                   or non-class space
//
// - MetaspaceCombinedStats: keeps reserved, committed and used byte counters, separately for both class- and non-class-space;
//                      retrieve with MetaspaceUtils::get_combined_statistics()

// (Note: just for NMT these objects need to be mutable)

class MetaspaceStats {
  Bytes _reserved;
  Bytes _committed;
  Bytes _used;
public:
  MetaspaceStats() : _reserved(Bytes(0)), _committed(Bytes(0)), _used(Bytes(0)) {}
  MetaspaceStats(Bytes r, Bytes c, Bytes u) : _reserved(r), _committed(c), _used(u) {}
  Bytes used() const       { return _used; }
  Bytes committed() const  { return _committed; }
  Bytes reserved() const   { return _reserved; }
};

// Class holds combined statistics for both non-class and class space.
class MetaspaceCombinedStats : public MetaspaceStats {
  MetaspaceStats _cstats;  // class space stats
  MetaspaceStats _ncstats; // non-class space stats
public:
  MetaspaceCombinedStats() {}
  MetaspaceCombinedStats(const MetaspaceStats& cstats, const MetaspaceStats& ncstats) :
    MetaspaceStats(cstats.reserved() + ncstats.reserved(),
                   cstats.committed() + ncstats.committed(),
                   cstats.used() + ncstats.used()),
    _cstats(cstats), _ncstats(ncstats)
  {}

  const MetaspaceStats& class_space_stats() const { return _cstats; }
  const MetaspaceStats& non_class_space_stats() const { return _ncstats; }
  Bytes class_used() const       { return _cstats.used(); }
  Bytes class_committed() const  { return _cstats.committed(); }
  Bytes class_reserved() const   { return _cstats.reserved(); }
  Bytes non_class_used() const       { return _ncstats.used(); }
  Bytes non_class_committed() const  { return _ncstats.committed(); }
  Bytes non_class_reserved() const   { return _ncstats.reserved(); }
};

#endif // SHARE_MEMORY_METASPACESTATS_HPP
