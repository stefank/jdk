/*
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2019, Twitter, Inc.
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

#ifndef SHARE_GC_SHARED_PREGCVALUES_HPP
#define SHARE_GC_SHARED_PREGCVALUES_HPP

#include "memory/metaspaceStats.hpp"
#include "memory/metaspaceUtils.hpp"

// Simple class for storing info about the heap at the start of GC, to be used
// after GC for comparison/printing.
class PreGenGCValues {
public:
  PreGenGCValues(Bytes young_gen_used,
                 Bytes young_gen_capacity,
                 Bytes eden_used,
                 Bytes eden_capacity,
                 Bytes from_used,
                 Bytes from_capacity,
                 Bytes old_gen_used,
                 Bytes old_gen_capacity)
      : _young_gen_used(young_gen_used),
        _young_gen_capacity(young_gen_capacity),
        _eden_used(eden_used),
        _eden_capacity(eden_capacity),
        _from_used(from_used),
        _from_capacity(from_capacity),
        _old_gen_used(old_gen_used),
        _old_gen_capacity(old_gen_capacity),
        _meta_sizes(MetaspaceUtils::get_combined_statistics()){ }

  Bytes young_gen_used()     const { return _young_gen_used;     }
  Bytes young_gen_capacity() const { return _young_gen_capacity; }
  Bytes eden_used()          const { return _eden_used;          }
  Bytes eden_capacity()      const { return _eden_capacity;      }
  Bytes from_used()          const { return _from_used;          }
  Bytes from_capacity()      const { return _from_capacity;      }
  Bytes old_gen_used()       const { return _old_gen_used;       }
  Bytes old_gen_capacity()   const { return _old_gen_capacity;   }
  const MetaspaceCombinedStats& metaspace_sizes() const { return _meta_sizes; }

private:
  const Bytes _young_gen_used;
  const Bytes _young_gen_capacity;
  const Bytes _eden_used;
  const Bytes _eden_capacity;
  const Bytes _from_used;
  const Bytes _from_capacity;
  const Bytes _old_gen_used;
  const Bytes _old_gen_capacity;
  const MetaspaceCombinedStats _meta_sizes;
};

#endif // SHARE_GC_SHARED_PREGCVALUES_HPP
