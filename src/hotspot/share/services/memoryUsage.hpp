/*
 * Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_SERVICES_MEMORYUSAGE_HPP
#define SHARE_SERVICES_MEMORYUSAGE_HPP

#include "utilities/globalDefinitions.hpp"

// A memory usage contains the following attributes about memory usage:
//  initSize - represents the initial amount of memory (in bytes) that
//     the Java virtual machine requests from the operating system
//     for memory management.  The Java virtual machine may request
//     additional memory from the operating system later when appropriate.
//     Its value may be undefined.
//  used      - represents the amount of memory currently used (in bytes).
//  committed - represents the amount of memory (in bytes) that is
//     guaranteed to be available for use by the Java virtual machine.
//     The amount of committed memory may change over time (increase
//     or decrease).  It is guaranteed to be greater than or equal
//     to initSize.
//  maxSize   - represents the maximum amount of memory (in bytes)
//     that can be used for memory management. The maximum amount of
//     memory for memory management could be less than the amount of
//     committed memory.  Its value may be undefined.

class MemoryUsage {
private:
  Bytes _initSize;
  Bytes _used;
  Bytes _committed;
  Bytes _maxSize;

public:
  // Constructors
  MemoryUsage(Bytes i, Bytes u, Bytes c, Bytes m) :
    _initSize(i), _used(u), _committed(c), _maxSize(m) {};
  MemoryUsage() :
    _initSize(Bytes(0)), _used(Bytes(0)), _committed(Bytes(0)), _maxSize(Bytes(0)) {};

  Bytes init_size() const { return _initSize; }
  Bytes used()      const { return _used; }
  Bytes committed() const { return _committed; }
  Bytes max_size()  const { return _maxSize; }

  static Bytes undefined_size() { return Bytes(SIZE_MAX); }

  inline static jlong convert_to_jlong(Bytes val) {
    // In the 64-bit vm, a size_t can overflow a jlong (which is signed).
    jlong ret;
    if (val == undefined_size()) {
      ret = -1L;
    } else {
      NOT_LP64(ret = val;)
      LP64_ONLY(ret = MIN2(untype(val), (size_t)max_jlong);)
    }
    return ret;
  }

  jlong init_size_as_jlong() const { return convert_to_jlong(_initSize); }
  jlong used_as_jlong()      const { return convert_to_jlong(_used); }
  jlong committed_as_jlong() const { return convert_to_jlong(_committed); }
  jlong max_size_as_jlong()  const { return convert_to_jlong(_maxSize); }
};

#endif // SHARE_SERVICES_MEMORYUSAGE_HPP
