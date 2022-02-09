/*
 * Copyright (c) 2017, 2021, Oracle and/or its affiliates. All rights reserved.
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
 */

#ifndef SHARE_GC_Z_SHARED_VMSTRUCTS_Z_SHARED_HPP
#define SHARE_GC_Z_SHARED_VMSTRUCTS_Z_SHARED_HPP

#include "gc/z/original/vmStructs_z_original.hpp"
#include "gc/z/vmStructs_z.hpp"

#define VM_STRUCTS_Z_SHARED(nonstatic_field, volatile_nonstatic_field, static_field)           \
  VM_STRUCTS_Z_ORIGINAL(                                                                       \
    nonstatic_field,                                                                           \
    volatile_nonstatic_field,                                                                  \
    static_field)                                                                              \
                                                                                               \
  VM_STRUCTS_Z_GENERATIONAL(                                                                   \
    nonstatic_field,                                                                           \
    volatile_nonstatic_field,                                                                  \
    static_field)

#define VM_INT_CONSTANTS_Z_SHARED(declare_constant, declare_constant_with_value)               \
  VM_INT_CONSTANTS_Z_ORIGINAL(                                                                 \
    declare_constant,                                                                          \
    declare_constant_with_value)                                                               \
                                                                                               \
  VM_INT_CONSTANTS_Z_GENERATIONAL(                                                             \
    declare_constant,                                                                          \
    declare_constant_with_value)

#define VM_LONG_CONSTANTS_Z_SHARED(declare_constant)                                           \
  VM_LONG_CONSTANTS_Z_ORIGINAL(                                                                \
    declare_constant)                                                                          \
                                                                                               \
  VM_LONG_CONSTANTS_Z_GENERATIONAL(                                                            \
    declare_constant)

#define VM_TYPES_Z_SHARED(declare_type, declare_toplevel_type, declare_integer_type)           \
  VM_TYPES_Z_ORIGINAL(                                                                         \
    declare_type,                                                                              \
    declare_toplevel_type,                                                                     \
    declare_integer_type)                                                                      \
                                                                                               \
  VM_TYPES_Z_GENERATIONAL(                                                                     \
    declare_type,                                                                              \
    declare_toplevel_type,                                                                     \
    declare_integer_type)

#endif // SHARE_GC_Z_SHARED_VMSTRUCTS_Z_SHARED_HPP
