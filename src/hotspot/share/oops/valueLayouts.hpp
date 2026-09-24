/*
 * Copyright (c) 2025, 2026, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_OOPS_VALUELAYOUTS_HPP
#define SHARE_OOPS_VALUELAYOUTS_HPP

#include "oops/layoutKind.hpp"

// The different layouts available for a particular value class.
class ValueLayouts {
  friend class FieldLayoutBuilder;
  friend class VMStructs;

  // Unsupported layouts are assigned this value.
  constexpr static int NoValue = -1;

  // Properties of buffered (boxed) values.
  int _payload_offset;
  int _payload_size_in_bytes;
  int _payload_alignment;

  int _null_marker_offset;

  // Size of each LayoutKind.
  int _sizes[LayoutKindCount];

  // Alignment of each LayoutKind.
  int _alignments[LayoutKindCount];

  // Private functions exposed to FieldLayoutBuilder.

  void set_size_in_bytes_of(LayoutKind lk, int value) {
    _sizes[static_cast<size_t>(lk)] = value;
  }

  void set_alignment_of(LayoutKind lk, int value) {
    _alignments[static_cast<size_t>(lk)] = value;
  }

  void set_payload_offset(int offset)       { _payload_offset = offset; }
  void set_payload_alignment(int alignment) { _payload_alignment = alignment; }
  void set_payload_size_in_bytes(int size)  { _payload_size_in_bytes = size; }
  void set_null_marker_offset(int offset)   { _null_marker_offset = offset; }

public:
  ValueLayouts()
    : _payload_offset(NoValue),
      _payload_size_in_bytes(NoValue),
      _payload_alignment(NoValue),
      _null_marker_offset(NoValue),
      _sizes() {
    // Make sizes and alignments uninitialized
    for (LayoutKind lk : EnumRange<LayoutKind>()) {
      set_size_in_bytes_of(lk, NoValue);
      set_alignment_of(lk, NoValue);
    }
  }

  // Returns default value if missing
  int size_in_bytes_of(LayoutKind lk) const {
    return _sizes[static_cast<size_t>(lk)];
  }

  int alignment_of(LayoutKind lk) const {
    return _alignments[static_cast<size_t>(lk)];
  }

  int payload_offset() const { return _payload_offset; }

  bool has_payload_alignment() const { return _payload_alignment != NoValue; }
  int  payload_alignment() const { return _payload_alignment; }

  int payload_size_in_bytes() const { return _payload_size_in_bytes; }

  int null_marker_offset() const { return _null_marker_offset; }
  int null_marker_offset_in_payload() const { return null_marker_offset() - payload_offset(); }

  bool has_a(LayoutKind lk) const {
    return size_in_bytes_of(lk) != NoValue;
  }

  template<typename... Ts>
  bool has_any(Ts... lks) const {
    return (has_a(lks) || ...);
  }

  static ByteSize payload_offset_offset() { return byte_offset_of(ValueLayouts, _payload_offset); }
  static ByteSize null_marker_offset_offset() { return byte_offset_of(ValueLayouts, _null_marker_offset); }

  void print_on(outputStream& st) const {
    for (LayoutKind lk : EnumRange<LayoutKind>()) {
      if (has_a(lk)) {
        st.print_cr("%s layout: %d/%d",
                    LayoutKindHelper::layout_kind_as_string(lk),
                    size_in_bytes_of(lk), alignment_of(lk));
      } else {
        st.print_cr("%s layout: -/-",
                    LayoutKindHelper::layout_kind_as_string(lk));
      }
    }
  }
};

#endif // SHARE_OOPS_VALUELAYOUTS_HPP
