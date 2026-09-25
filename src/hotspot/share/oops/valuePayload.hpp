/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_OOPS_VALUEPAYLOAD_HPP
#define SHARE_VM_OOPS_VALUEPAYLOAD_HPP

#include "oops/instanceKlass.hpp"
#include "oops/layoutKind.hpp"
#include "oops/oopHandle.hpp"
#include "oops/oopsHierarchy.hpp"
#include "oops/valueOop.hpp"
#include "runtime/handles.hpp"
#include "utilities/exceptions.hpp"
#include "utilities/globalDefinitions.hpp"
#include "utilities/macros.hpp"
#include "utilities/ostream.hpp"

class fieldDescriptor;
class JavaThread;
class outputStream;
class ResolvedFieldEntry;

class ValuePayloadLayout {
  OptionalFlatLayout _optional_flat_layout;

  ValuePayloadLayout(OptionalFlatLayout optional_flat_layout)
    : _optional_flat_layout(optional_flat_layout) {}

  bool is_initialized() const {
    return _optional_flat_layout.is_initialized();
  }

public:
  static ValuePayloadLayout flat(LayoutKind layout_kind) {
    return ValuePayloadLayout(OptionalFlatLayout::flat(layout_kind));
  }

  static ValuePayloadLayout buffered() {
    return ValuePayloadLayout(OptionalFlatLayout::non_flat());
  }

  static ValuePayloadLayout uninitialized() {
    return ValuePayloadLayout(OptionalFlatLayout::uninitialized());
  }

  bool is_buffered() const {
    precond(is_initialized());
    return !_optional_flat_layout.is_flat();
  }

  FlatLayout flat_layout() const {
    precond(is_initialized());
    return _optional_flat_layout.get();
  }

  LayoutKind layout_kind() const {
    precond(is_initialized());
    return flat_layout().layout_kind();
  }
};

class ValuePayload {
private:
  template <typename OopOrHandle> class StorageImpl {
  private:
    union {
      struct {
        OopOrHandle _container;
        ptrdiff_t _offset;
      };
      address _absolute_addr;
    };
    ValueKlass* _klass;
    ValuePayloadLayout _layout;
    bool _uses_absolute_addr;

  public:
    inline StorageImpl(OopOrHandle container,
                       ptrdiff_t offset,
                       ValueKlass* klass,
                       ValuePayloadLayout layout);
    inline StorageImpl(address absolute_addr,
                       ValueKlass* klass,
                       ValuePayloadLayout);
    inline ~StorageImpl();
    inline StorageImpl(const StorageImpl& other);
    inline StorageImpl& operator=(const StorageImpl& other);

    inline OopOrHandle& container();
    inline OopOrHandle container() const;

    inline ptrdiff_t& offset();
    inline ptrdiff_t offset() const;

    inline address& absolute_addr();
    inline address absolute_addr() const;

    inline ValueKlass* klass() const;

    inline ValuePayloadLayout layout() const;

    inline bool is_buffered() const;

    inline FlatLayout flat_layout() const;
    inline LayoutKind layout_kind() const;

    inline bool uses_absolute_addr() const;
  };

  using Storage = StorageImpl<oop>;

  Storage _storage;

protected:
  static constexpr ptrdiff_t BAD_OFFSET = -1;

  ValuePayload(const ValuePayload&) = default;
  ValuePayload& operator=(const ValuePayload&) = default;

  // Constructed from parts container and offset
  inline ValuePayload(oop container,
                      ptrdiff_t offset,
                      ValueKlass* klass,
                      ValuePayloadLayout layout);

  // Constructed from parts absolute_addr
  inline ValuePayload(address absolute_addr,
                      ValueKlass* klass,
                      ValuePayloadLayout layout);

  inline bool is_buffered() const;

  inline ValuePayloadLayout layout() const;
  inline FlatLayout flat_layout() const;
  inline LayoutKind flat_layout_kind() const;

  inline void set_offset(ptrdiff_t offset);

  static inline void copy(const ValuePayload& src,
                          const ValuePayload& dst);

  inline void mark_as_non_null();
  inline void mark_as_null();

  inline bool uses_absolute_addr() const;

  inline oop& container();
  inline oop container() const;

  inline void print_on(outputStream* st) const NOT_DEBUG_RETURN;

private:
  inline void assert_is_flat_field(const InstanceKlass* klass, int offset) const NOT_DEBUG_RETURN;
  inline void assert_post_construction_invariants() const NOT_DEBUG_RETURN;
  static inline void assert_pre_copy_invariants(const ValuePayload& src,
                                                const ValuePayload& dst) NOT_DEBUG_RETURN;

public:
  inline ValueKlass* klass() const;
  inline ptrdiff_t offset() const;

  inline address addr() const;

  inline bool has_null_marker() const;
  inline bool is_payload_null() const;

  inline bool is_nullable_flat() const;
  inline bool is_atomic_flat() const;

  inline int size_in_bytes() const;

  class Handle;
  class OopHandle;

  static inline int copy_size_in_bytes(const ValuePayload& src, const ValuePayload& dst);
};

class BufferedValuePayload : public ValuePayload {
  friend class FlatValuePayload;

private:
  inline BufferedValuePayload(valueOop container,
                              ptrdiff_t offset,
                              ValueKlass* klass);

public:
  BufferedValuePayload(const BufferedValuePayload&) = default;
  BufferedValuePayload& operator=(const BufferedValuePayload&) = default;

  explicit inline BufferedValuePayload(valueOop buffer);
  inline BufferedValuePayload(valueOop buffer, ValueKlass* klass);

  inline valueOop container() const;

  inline void copy_to(const BufferedValuePayload& dst);

  class Handle;
  class OopHandle;

  inline Handle make_handle(JavaThread* thread) const;
  inline OopHandle make_oop_handle(OopStorage* storage) const;
};

class FlatValuePayload : public ValuePayload {
protected:
  inline FlatValuePayload(oop container,
                          ptrdiff_t offset,
                          ValueKlass* klass,
                          LayoutKind layout_kind);

  inline FlatValuePayload(address absolute_addr,
                          ValueKlass* klass,
                          LayoutKind layout_kind);

private:
  inline valueOop allocate_instance(TRAPS);

public:
  FlatValuePayload(const FlatValuePayload&) = default;
  FlatValuePayload& operator=(const FlatValuePayload&) = default;

  [[nodiscard]] inline bool copy_to(BufferedValuePayload& dst);
  inline void copy_from(BufferedValuePayload& src);

  inline void copy_to(const FlatValuePayload& dst);

  [[nodiscard]] inline valueOop read(TRAPS);
  inline void write_without_nullability_check(valueOop obj);
  inline void write(valueOop obj, TRAPS);

  [[nodiscard]] static inline FlatValuePayload construct_from_parts(oop container,
                                                                    ptrdiff_t offset,
                                                                    ValueKlass* klass,
                                                                    LayoutKind layout_kind);

  [[nodiscard]] static inline FlatValuePayload construct_from_parts(address absolute_addr,
                                                                    ValueKlass* klass,
                                                                    LayoutKind layout_kind);

  class Handle;
  class OopHandle;

  inline Handle make_handle(JavaThread* thread) const;
  inline OopHandle make_oop_handle(OopStorage* storage) const;
};

class FlatFieldPayload : public FlatValuePayload {
private:
  inline FlatFieldPayload(instanceOop container,
                          ptrdiff_t offset,
                          ValueKlass* klass,
                          LayoutKind layout_kind);

  inline FlatFieldPayload(instanceOop container,
                          ptrdiff_t offset,
                          ValueFieldInfo* value_field_info);

  inline void assert_post_construction_invariants(instanceOop container,
                                                  ResolvedFieldEntry* resolved_field_entry) const NOT_DEBUG_RETURN;
  inline void assert_post_construction_invariants(instanceOop container,
                                                  fieldDescriptor* field_descriptor) const NOT_DEBUG_RETURN;

public:
  inline FlatFieldPayload(instanceOop container,
                          fieldDescriptor* field_descriptor);

  inline FlatFieldPayload(instanceOop container,
                          ResolvedFieldEntry* resolved_field_entry);

  inline instanceOop container() const;

  class Handle;
  class OopHandle;

  inline Handle make_handle(JavaThread* thread) const;
  inline OopHandle make_oop_handle(OopStorage* storage) const;
};

class FlatArrayPayload : public FlatValuePayload {
private:
  struct Storage {
    jint _layout_helper;
    int _element_size;
  } _storage;

  inline FlatArrayPayload(flatArrayOop container,
                          ptrdiff_t offset,
                          ValueKlass* klass,
                          LayoutKind layout_kind,
                          jint layout_helper,
                          int element_size);

public:
  FlatArrayPayload(const FlatArrayPayload&) = default;
  FlatArrayPayload& operator=(const FlatArrayPayload&) = default;

  explicit inline FlatArrayPayload(flatArrayOop container);
  inline FlatArrayPayload(flatArrayOop container, FlatArrayKlass* klass);

  inline FlatArrayPayload(flatArrayOop container, int index);
  inline FlatArrayPayload(flatArrayOop container, int index, FlatArrayKlass* klass);

  inline flatArrayOop container() const;

  inline void set_index(int index);
  inline void advance_index(int delta);

  inline void next_element();
  inline void previous_element();

private:
  inline void set_offset(ptrdiff_t offset);

public:
  class Handle;
  class OopHandle;

  inline Handle make_handle(JavaThread* thread) const;
  inline OopHandle make_oop_handle(OopStorage* storage) const;
};

class ValuePayload::Handle {
private:
  using Storage = StorageImpl<::Handle>;

  Storage _storage;

protected:
  inline Handle(const ValuePayload& payload, JavaThread* thread);

  inline oop container() const;

  inline ValuePayloadLayout layout() const;

public:
  Handle(const Handle&) = default;
  Handle& operator=(const Handle&) = default;

  inline ValueKlass* klass() const;
  inline ptrdiff_t offset() const;
};

class ValuePayload::OopHandle {
private:
  using Storage = StorageImpl<::OopHandle>;

  Storage _storage;

protected:
  inline OopHandle(const ValuePayload& payload, OopStorage* storage);

  inline oop container() const;

  inline ValuePayloadLayout layout() const;

public:
  OopHandle(const OopHandle&) = default;
  OopHandle& operator=(const OopHandle&) = default;

  inline void release(OopStorage* storage);

  inline ValueKlass* klass() const;
  inline ptrdiff_t offset() const;
};

class BufferedValuePayload::Handle : public ValuePayload::Handle {
public:
  inline Handle(const BufferedValuePayload& payload, JavaThread* thread);

  inline BufferedValuePayload operator()() const;

  inline valueOop container() const;
};

class BufferedValuePayload::OopHandle : public ValuePayload::OopHandle {
public:
  inline OopHandle(const BufferedValuePayload& payload, OopStorage* storage);

  inline BufferedValuePayload operator()() const;

  inline valueOop container() const;
};

class FlatValuePayload::Handle : public ValuePayload::Handle {
public:
  inline Handle(const FlatValuePayload& payload, JavaThread* thread);

  inline FlatValuePayload operator()() const;
};

class FlatValuePayload::OopHandle : public ValuePayload::OopHandle {
public:
  inline OopHandle(const FlatValuePayload& payload, OopStorage* storage);

  inline FlatValuePayload operator()() const;
};

class FlatFieldPayload::Handle : public FlatValuePayload::Handle {
public:
  inline Handle(const FlatFieldPayload& payload, JavaThread* thread);

  inline FlatFieldPayload operator()() const;

  inline instanceOop container() const;
};

class FlatFieldPayload::OopHandle : public FlatValuePayload::OopHandle {
public:
  inline OopHandle(const FlatFieldPayload& payload, OopStorage* storage);

  inline FlatFieldPayload operator()() const;

  inline instanceOop container() const;
};

class FlatArrayPayload::Handle : public FlatValuePayload::Handle {
private:
  FlatArrayPayload::Storage _storage;

public:
  inline Handle(const FlatArrayPayload& payload, JavaThread* thread);

  inline FlatArrayPayload operator()() const;

  inline flatArrayOop container() const;
};

class FlatArrayPayload::OopHandle : public FlatValuePayload::OopHandle {
private:
  FlatArrayPayload::Storage _storage;

public:
  inline OopHandle(const FlatArrayPayload& payload, OopStorage* storage);

  inline FlatArrayPayload operator()() const;

  inline flatArrayOop container() const;
};

#endif // SHARE_VM_OOPS_VALUEPAYLOAD_HPP
