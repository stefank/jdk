/*
 * Copyright (c) 1997, 2022, Oracle and/or its affiliates. All rights reserved.
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
#include "memory/allocation.hpp"
#include "memory/allocation.inline.hpp"
#include "memory/arena.hpp"
#include "memory/metaspace.hpp"
#include "memory/resourceArea.hpp"
#include "runtime/os.hpp"
#include "runtime/task.hpp"
#include "runtime/threadCritical.hpp"
#include "services/memTracker.hpp"
#include "utilities/ostream.hpp"

// allocate using malloc; will fail if no memory available
char* AllocateHeap(size_t size,
                   MEMFLAGS flags,
                   const NativeCallStack& stack,
                   AllocFailType alloc_failmode /* = AllocFailStrategy::EXIT_OOM*/) {
  char* p = (char*) os::malloc(size, flags, stack);
  if (p == NULL && alloc_failmode == AllocFailStrategy::EXIT_OOM) {
    vm_exit_out_of_memory(size, OOM_MALLOC_ERROR, "AllocateHeap");
  }
  return p;
}

char* AllocateHeap(size_t size,
                   MEMFLAGS flags,
                   AllocFailType alloc_failmode /* = AllocFailStrategy::EXIT_OOM*/) {
  return AllocateHeap(size, flags, CALLER_PC, alloc_failmode);
}

char* ReallocateHeap(char *old,
                     size_t size,
                     MEMFLAGS flag,
                     AllocFailType alloc_failmode) {
  char* p = (char*) os::realloc(old, size, flag, CALLER_PC);
  if (p == NULL && alloc_failmode == AllocFailStrategy::EXIT_OOM) {
    vm_exit_out_of_memory(size, OOM_MALLOC_ERROR, "ReallocateHeap");
  }
  return p;
}

// handles NULL pointers
void FreeHeap(void* p) {
  os::free(p);
}

void* MetaspaceObj::_shared_metaspace_base = NULL;
void* MetaspaceObj::_shared_metaspace_top  = NULL;

void* StackObj::operator new(size_t size)     throw() { ShouldNotCallThis(); return 0; }
void  StackObj::operator delete(void* p)              { ShouldNotCallThis(); }
void* StackObj::operator new [](size_t size)  throw() { ShouldNotCallThis(); return 0; }
void  StackObj::operator delete [](void* p)           { ShouldNotCallThis(); }

void* MetaspaceObj::operator new(size_t size, ClassLoaderData* loader_data,
                                 size_t word_size,
                                 MetaspaceObj::Type type, TRAPS) throw() {
  // Klass has its own operator new
  return Metaspace::allocate(loader_data, word_size, type, THREAD);
}

void* MetaspaceObj::operator new(size_t size, ClassLoaderData* loader_data,
                                 size_t word_size,
                                 MetaspaceObj::Type type) throw() {
  assert(!Thread::current()->is_Java_thread(), "only allowed by non-Java thread");
  return Metaspace::allocate(loader_data, word_size, type);
}

bool MetaspaceObj::is_valid(const MetaspaceObj* p) {
  // Weed out obvious bogus values first without traversing metaspace
  if ((size_t)p < os::min_page_size()) {
    return false;
  } else if (!is_aligned((address)p, sizeof(MetaWord))) {
    return false;
  }
  return Metaspace::contains((void*)p);
}

void MetaspaceObj::print_address_on(outputStream* st) const {
  st->print(" {" PTR_FORMAT "}", p2i(this));
}

void* ResourceObj::operator new(size_t size, Arena *arena) throw() {
  return arena->Amalloc(size);
}

void* ResourceObj::operator new(size_t size, MEMFLAGS flags) throw() {
  return AllocateHeap(size, flags, CALLER_PC);
}

void* ResourceObj::operator new(size_t size, const std::nothrow_t&  nothrow_constant, MEMFLAGS flags) throw() {
  // should only call this with std::nothrow, use other operator new() otherwise
  return AllocateHeap(size, flags, CALLER_PC, AllocFailStrategy::RETURN_NULL);
}

void ResourceObj::operator delete(void* p) {
  FreeHeap(p);
}

//--------------------------------------------------------------------------------------
// Non-product code

#ifndef PRODUCT
void ResourceObj::print() const       { print_on(tty); }

void ResourceObj::print_on(outputStream* st) const {
  st->print_cr("ResourceObj(" PTR_FORMAT ")", p2i(this));
}

ReallocMark::ReallocMark() {
#ifdef ASSERT
  Thread *thread = Thread::current();
  _nesting = thread->resource_area()->nesting();
#endif
}

void ReallocMark::check() {
#ifdef ASSERT
  if (_nesting != Thread::current()->resource_area()->nesting()) {
    fatal("allocation bug: array could grow within nested ResourceMark");
  }
#endif
}

#endif // Non-product
