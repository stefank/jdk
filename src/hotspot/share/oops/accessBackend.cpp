/*
 * Copyright (c) 2017, 2025, Oracle and/or its affiliates. All rights reserved.
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

#include "oops/accessBackend.hpp"
#include "accessBackend.inline.hpp"
#include "gc/shared/collectedHeap.hpp"
#include "oops/accessDecorators.hpp"
#include "oops/oop.inline.hpp"
#include "oops/valuePayload.hpp"
#include "runtime/javaThread.inline.hpp"
#include "runtime/mutexLocker.hpp"
#include "utilities/copy.hpp"
#include "utilities/debug.hpp"
#include "utilities/powerOfTwo.hpp"
#include "utilities/vmError.hpp"

#if defined(TARGET_COMPILER_gcc)
#define HIDDEN __attribute__ ((visibility ("hidden")))
#else
#define HIDDEN
#endif

namespace AccessInternal {
// These forward copying calls to Copy without exposing the Copy type in headers unnecessarily

  void arraycopy_arrayof_conjoint_oops(void* src, void* dst, size_t length) {
    Copy::arrayof_conjoint_oops(reinterpret_cast<HeapWord*>(src),
                                reinterpret_cast<HeapWord*>(dst), length);
  }

  void arraycopy_conjoint_oops(oop* src, oop* dst, size_t length) {
    Copy::conjoint_oops_atomic(src, dst, length);
  }

  void arraycopy_conjoint_oops(narrowOop* src, narrowOop* dst, size_t length) {
    Copy::conjoint_oops_atomic(src, dst, length);
  }

  void arraycopy_disjoint_words(void* src, void* dst, size_t length) {
    Copy::disjoint_words(reinterpret_cast<HeapWord*>(src),
                         reinterpret_cast<HeapWord*>(dst), length);
  }

  void arraycopy_disjoint_words_atomic(void* src, void* dst, size_t length) {
    Copy::disjoint_words_atomic(reinterpret_cast<HeapWord*>(src),
                                reinterpret_cast<HeapWord*>(dst), length);
  }

  template<> HIDDEN
  void arraycopy_conjoint<jboolean>(jboolean* src, jboolean* dst, size_t length) {
    Copy::conjoint_jbytes(reinterpret_cast<jbyte*>(src), reinterpret_cast<jbyte*>(dst), length);
  }

  template<> HIDDEN
  void arraycopy_conjoint<jbyte>(jbyte* src, jbyte* dst, size_t length) {
    Copy::conjoint_jbytes(src, dst, length);
  }

  template<> HIDDEN
  void arraycopy_conjoint<jchar>(jchar* src, jchar* dst, size_t length) {
    Copy::conjoint_jshorts_atomic(reinterpret_cast<jshort*>(src), reinterpret_cast<jshort*>(dst), length);
  }

  template<> HIDDEN
  void arraycopy_conjoint<jshort>(jshort* src, jshort* dst, size_t length) {
    Copy::conjoint_jshorts_atomic(src, dst, length);
  }

  template<> HIDDEN
  void arraycopy_conjoint<jint>(jint* src, jint* dst, size_t length) {
    Copy::conjoint_jints_atomic(src, dst, length);
  }

  template<> HIDDEN
  void arraycopy_conjoint<jfloat>(jfloat* src, jfloat* dst, size_t length) {
    Copy::conjoint_jints_atomic(reinterpret_cast<jint*>(src), reinterpret_cast<jint*>(dst), length);
  }

  template<> HIDDEN
  void arraycopy_conjoint<jlong>(jlong* src, jlong* dst, size_t length) {
    Copy::conjoint_jlongs_atomic(src, dst, length);
  }

  template<> HIDDEN
  void arraycopy_conjoint<jdouble>(jdouble* src, jdouble* dst, size_t length) {
    Copy::conjoint_jlongs_atomic(reinterpret_cast<jlong*>(src), reinterpret_cast<jlong*>(dst), length);
  }

  template<> HIDDEN
  void arraycopy_arrayof_conjoint<jbyte>(jbyte* src, jbyte* dst, size_t length) {
    Copy::arrayof_conjoint_jbytes(reinterpret_cast<HeapWord*>(src),
                                  reinterpret_cast<HeapWord*>(dst),
                                  length);
  }

  template<> HIDDEN
  void arraycopy_arrayof_conjoint<jshort>(jshort* src, jshort* dst, size_t length) {
    Copy::arrayof_conjoint_jshorts(reinterpret_cast<HeapWord*>(src),
                                   reinterpret_cast<HeapWord*>(dst),
                                   length);
  }

  template<> HIDDEN
  void arraycopy_arrayof_conjoint<jint>(jint* src, jint* dst, size_t length) {
    Copy::arrayof_conjoint_jints(reinterpret_cast<HeapWord*>(src),
                                 reinterpret_cast<HeapWord*>(dst),
                                 length);
  }

  template<> HIDDEN
  void arraycopy_arrayof_conjoint<jlong>(jlong* src, jlong* dst, size_t length) {
    Copy::arrayof_conjoint_jlongs(reinterpret_cast<HeapWord*>(src),
                                  reinterpret_cast<HeapWord*>(dst),
                                  length);
  }

  template<> HIDDEN
  void arraycopy_conjoint<void>(void* src, void* dst, size_t length) {
    Copy::conjoint_jbytes(reinterpret_cast<jbyte*>(src),
                          reinterpret_cast<jbyte*>(dst),
                          length);
  }

  template<> HIDDEN
  void arraycopy_conjoint_atomic<jbyte>(jbyte* src, jbyte* dst, size_t length) {
    Copy::conjoint_jbytes_atomic(src, dst, length);
  }

  template<> HIDDEN
  void arraycopy_conjoint_atomic<jshort>(jshort* src, jshort* dst, size_t length) {
    Copy::conjoint_jshorts_atomic(src, dst, length);
  }

  template<> HIDDEN
  void arraycopy_conjoint_atomic<jint>(jint* src, jint* dst, size_t length) {
    Copy::conjoint_jints_atomic(src, dst, length);
  }

  template<> HIDDEN
  void arraycopy_conjoint_atomic<jlong>(jlong* src, jlong* dst, size_t length) {
    Copy::conjoint_jlongs_atomic(src, dst, length);
  }

  template<> HIDDEN
  void arraycopy_conjoint_atomic<void>(void* src, void* dst, size_t length) {
    Copy::conjoint_memory_atomic(src, dst, length);
  }

  void value_copy_internal(void* src, void* dst, size_t length) {
    Copy::copy_value_content(src, dst, length);
  }

  void value_store_null(void* dst, size_t length) {
    Copy::clear_value_content(dst, length);
  }

  template <typename T>
  bool compare_exchange(void* addr, void* witness_addr, void* new_value_addr) {
    T witness_value = *(T*)witness_addr;
    T new_value = *(T*)new_value_addr;
    T ret = AtomicAccess::cmpxchg<T>((T*)addr, witness_value, new_value);
    return ret == witness_value;
  }

  bool compare_exchange(void* addr, void* witness_addr, void* new_value_addr, size_t size) {
    switch (size) {
      case 8: return compare_exchange<uint64_t>(addr, witness_addr, new_value_addr);
      case 4: return compare_exchange<uint32_t>(addr, witness_addr, new_value_addr);
      default: ShouldNotReachHere(); return false;
    }
  }

  template <typename T>
  void copy_raw(void* from, void* to) {
    *(T*)to = *(T*)from;
  }

  void copy_raw(void* from, void* to, size_t size) {
    switch (size) {
      case 8: copy_raw<uint64_t>(from, to); break;
      case 4: copy_raw<uint32_t>(from, to); break;
      default: ShouldNotReachHere(); break;
    }
  }

  template <typename T>
  void copy_seq_cst(void* from, void* to) {
    T value = RawAccess<MO_SEQ_CST>::load((volatile T*)from);
    *(T*)to = value;
  }

  void copy_seq_cst(void* from, void* to, size_t size) {
    switch (size) {
      case 8: copy_seq_cst<uint64_t>(from, to); break;
      case 4: copy_seq_cst<uint32_t>(from, to); break;
      default: ShouldNotReachHere(); break;
    }
  }

  void value_atomic_cmpxchg(ValuePayload& dst, const ValuePayload& compare_value, const ValuePayload& new_value, ValuePayload& witness) {
    const size_t size = ValuePayload::copy_size_in_bytes(dst, witness);

    // This function is used to implement cmpxchg for flattened values with oop.
    // There's no generic 2-byte cmpxchg support in the HotSpot code, so limit
    // the implementation to those sizes that we can encounter.
    assert(size == 4 || size == 8, "Unexpected size: %zu", size);

    union {
      uint64_t _v64;
      uint32_t _v32;
    } raw_bits;

    do {
      // Copy to raw_bits
      copy_seq_cst(dst.addr(), &raw_bits, size);

      // Set up the witness
      copy_raw(&raw_bits, witness.addr(), size);

      if (dst.klass()->supports_nullable_layouts() && !dst.is_nullable_flat()) {
        // Null-marker is not present at the source, and garbage padding bits can
        // have been written to the null-marker. Fix the null-marker.
        dst.klass()->mark_payload_as_non_null(witness.addr());
      }

      if (!witness.equals(compare_value)) {
        // Cas will not succeed - witness filled with previous value
        return;
      }

      if (compare_exchange(dst.addr(), &raw_bits, new_value.addr(), size)) {
        // Succeeded - witness filled with previous value
        return;
      }
    } while (true);
  }

  void value_atomic_xchg(ValuePayload& dst, const ValuePayload& new_value, ValuePayload& witness) {
    Unimplemented();
  }

#ifdef ASSERT
  void check_access_thread_state() {
    if (VMError::is_error_reported() || DebuggingContext::is_enabled()) {
      return;
    }

    Thread* thread = Thread::current();
    if (!thread->is_Java_thread()) {
      return;
    }

    JavaThread* java_thread = JavaThread::cast(thread);
    JavaThreadState state = java_thread->thread_state();
    assert(state == _thread_in_vm || state == _thread_in_Java || state == _thread_new,
           "Wrong thread state for accesses: %d", (int)state);
  }
#endif
}
