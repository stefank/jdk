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

#include "precompiled.hpp"
#include "code/codeCache.hpp"
#include "code/relocInfo.hpp"
#include "code/nmethod.hpp"
#include "code/icBuffer.hpp"
#include "gc/shared/barrierSet.hpp"
#include "gc/shared/barrierSetNMethod.hpp"
#include "gc/shared/suspendibleThreadSet.hpp"
#include "gc/z/zAddress.hpp"
#include "gc/z/zArray.inline.hpp"
#include "gc/z/zBarrier.inline.hpp"
#include "gc/z/zBarrierSet.hpp"
#include "gc/z/zBarrierSetAssembler.hpp"
#include "gc/z/zBarrierSetNMethod.hpp"
#include "gc/z/zCollector.inline.hpp"
#include "gc/z/zLock.inline.hpp"
#include "gc/z/zNMethod.hpp"
#include "gc/z/zNMethodData.hpp"
#include "gc/z/zNMethodTable.hpp"
#include "gc/z/zTask.hpp"
#include "gc/z/zTracer.inline.hpp"
#include "gc/z/zUncoloredRoot.inline.hpp"
#include "gc/z/zWorkers.hpp"
#include "logging/log.hpp"
#include "memory/allocation.inline.hpp"
#include "memory/iterator.hpp"
#include "memory/resourceArea.hpp"
#include "memory/universe.hpp"
#include "oops/klass.inline.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/atomic.hpp"
#include "utilities/debug.hpp"

static ZNMethodData* gc_data(const nmethod* nm) {
  return nm->gc_data<ZNMethodData>();
}

static void set_gc_data(nmethod* nm, ZNMethodData* data) {
  return nm->set_gc_data<ZNMethodData>(data);
}

void ZNMethod::attach_gc_data(nmethod* nm) {
  ZArray<ZNMethodDataBarrier> barriers;
  ZArray<oop*> immediate_oops;
  bool has_non_immediate_oops = false;

  // Find all barrier and oop relocations
  RelocIterator iter(nm);
  while (iter.next()) {
    if (iter.type() == relocInfo::barrier_type) {
      // Barrier relocation
      barrier_Relocation* const reloc = iter.barrier_reloc();
      barriers.push({ reloc->addr(), reloc->format() });
    } else if (iter.type() == relocInfo::oop_type) {
      // Oop relocation
      oop_Relocation* const reloc = iter.oop_reloc();

      if (!reloc->oop_is_immediate()) {
        // Non-immediate oop found
        has_non_immediate_oops = true;
      } else if (reloc->oop_value() != NULL) {
        // Non-NULL immediate oop found. NULL oops can safely be
        // ignored since the method will be re-registered if they
        // are later patched to be non-NULL.
        immediate_oops.push(reloc->oop_addr());
      }
    }
  }

  // Attach GC data to nmethod
  ZNMethodData* data = gc_data(nm);
  if (data == NULL) {
    data = new ZNMethodData();
    set_gc_data(nm, data);
  }

  // Attach barriers and oops to GC data
  data->swap(&barriers, &immediate_oops, has_non_immediate_oops);
}

ZReentrantLock* ZNMethod::lock_for_nmethod(nmethod* nm) {
  return gc_data(nm)->lock();
}

void ZNMethod::log_register(const nmethod* nm) {
  LogTarget(Debug, gc, nmethod) log;
  if (!log.is_enabled()) {
    return;
  }

  const ZNMethodData* const data = gc_data(nm);

  log.print("Register NMethod: %s.%s (" PTR_FORMAT ") [" PTR_FORMAT ", " PTR_FORMAT "] "
            "Compiler: %s, Barriers: %d, Oops: %d, ImmediateOops: %d, NonImmediateOops: %s",
            nm->method()->method_holder()->external_name(),
            nm->method()->name()->as_C_string(),
            p2i(nm),
            p2i(nm->code_begin()),
            p2i(nm->code_end()),
            nm->compiler_name(),
            data->barriers()->length(),
            nm->oops_count() - 1,
            data->immediate_oops()->length(),
            data->has_non_immediate_oops() ? "Yes" : "No");

  LogTarget(Trace, gc, nmethod, barrier) log_barriers;
  if (log_barriers.is_enabled()) {
    // Print nmethod barriers
    ZArrayIterator<ZNMethodDataBarrier> iter(data->barriers());
    for (ZNMethodDataBarrier b; iter.next(&b);) {
      log_barriers.print("       Barrier: %d @ " PTR_FORMAT,
                         b._reloc_format, p2i(b._reloc_addr));
    }
  }

  LogTarget(Trace, gc, nmethod, oops) log_oops;
  if (log_oops.is_enabled()) {
    // Print nmethod oops table
    oop* const begin = nm->oops_begin();
    oop* const end = nm->oops_end();
    for (oop* p = begin; p < end; p++) {
      log_oops.print("           Oop: " PTR_FORMAT " (%s)",
                     p2i(*p), (*p)->klass()->external_name());
    }

    // Print nmethod immediate oops
    ZArrayIterator<oop*> iter(data->immediate_oops());
    for (oop* p; iter.next(&p);) {
      log_oops.print("  ImmediateOop: " PTR_FORMAT " @ " PTR_FORMAT " (%s)",
                     p2i(*p), p2i(p), (*p)->klass()->external_name());
    }
  }
}

void ZNMethod::log_unregister(const nmethod* nm) {
  LogTarget(Debug, gc, nmethod) log;
  if (!log.is_enabled()) {
    return;
  }

  log.print("Unregister NMethod: %s.%s (" PTR_FORMAT ") [" PTR_FORMAT ", " PTR_FORMAT "] ",
            nm->method()->method_holder()->external_name(),
            nm->method()->name()->as_C_string(),
            p2i(nm),
            p2i(nm->code_begin()),
            p2i(nm->code_end()));
}

void ZNMethod::register_nmethod(nmethod* nm) {
  ResourceMark rm;

  // Create and attach gc data
  attach_gc_data(nm);

  ZTraceThreadEvent event("ZNMethod register_nmethod");
  ZLocker<ZReentrantLock> locker(lock_for_nmethod(nm));

  log_register(nm);

  // Patch nmathod barriers
  nmethod_patch_barriers(nm);

  // Register nmethod
  ZNMethodTable::register_nmethod(nm);

  // Disarm nmethod entry barrier
  disarm(nm);
}

void ZNMethod::unregister_nmethod(nmethod* nm) {
  assert(CodeCache_lock->owned_by_self(), "Lock must be held");

  if (Thread::current()->is_Code_cache_sweeper_thread()) {
    // The sweeper must wait for any ongoing iteration to complete
    // before it can unregister an nmethod.
    ZNMethodTable::wait_until_iteration_done();
  }

  ResourceMark rm;

  log_unregister(nm);

  ZNMethodTable::unregister_nmethod(nm);
}

void ZNMethod::flush_nmethod(nmethod* nm) {
  // Destroy GC data
  delete gc_data(nm);
}

bool ZNMethod::supports_entry_barrier(nmethod* nm) {
  BarrierSetNMethod* const bs = BarrierSet::barrier_set()->barrier_set_nmethod();
  return bs->supports_entry_barrier(nm);
}

bool ZNMethod::is_armed(nmethod* nm) {
  BarrierSetNMethod* const bs = BarrierSet::barrier_set()->barrier_set_nmethod();
  return bs->is_armed(nm);
}

void ZNMethod::disarm(nmethod* nm) {
  BarrierSetNMethod* const bs = BarrierSet::barrier_set()->barrier_set_nmethod();
  bs->disarm(nm);
}

void ZNMethod::nmethod_patch_barriers(nmethod* nm) {
  ZBarrierSetAssembler* const bs_asm = ZBarrierSet::assembler();
  ZArrayIterator<ZNMethodDataBarrier> iter(gc_data(nm)->barriers());
  for (ZNMethodDataBarrier barrier; iter.next(&barrier);) {
    bs_asm->patch_barrier_relocation(barrier._reloc_addr, barrier._reloc_format);
  }
}

void ZNMethod::nmethod_oops_do(nmethod* nm, OopClosure* cl) {
  ZTraceThreadEvent event("ZNMethod nmethod_oops_do");
  ZLocker<ZReentrantLock> locker(lock_for_nmethod(nm));
  if (!nm->is_alive()) {
    return;
  }

  ZNMethod::nmethod_oops_do_inner(nm, cl);
}

void ZNMethod::nmethod_oops_do_inner(nmethod* nm, OopClosure* cl) {
  // Process oops table
  {
    oop* const begin = nm->oops_begin();
    oop* const end = nm->oops_end();
    for (oop* p = begin; p < end; p++) {
      if (!Universe::contains_non_oop_word(p)) {
        cl->do_oop(p);
      }
    }
  }

  ZNMethodData* const data = gc_data(nm);

  // Process immediate oops
  {
    ZArrayIterator<oop*> iter(data->immediate_oops());
    for (oop* p; iter.next(&p);) {
      if (!Universe::contains_non_oop_word(p)) {
        cl->do_oop(p);
      }
    }
  }

  // Process non-immediate oops
  if (data->has_non_immediate_oops()) {
    nm->fix_oop_relocations();
  }
}

void ZNMethod::nmethods_do_begin(bool secondary) {
  ZNMethodTable::nmethods_do_begin(secondary);
}

void ZNMethod::nmethods_do_end(bool secondary) {
  ZNMethodTable::nmethods_do_end(secondary);
}

void ZNMethod::nmethods_do(bool secondary, NMethodClosure* cl) {
  ZNMethodTable::nmethods_do(secondary, cl);
}

uintptr_t ZNMethod::color(nmethod* nm) {
  BarrierSetNMethod* bs_nm = BarrierSet::barrier_set()->barrier_set_nmethod();
  // color is stored at low order bits of int; implicit conversion to uintptr_t is fine
  return bs_nm->arm_value(nm);
}

oop ZNMethod::load_oop(oop* p, DecoratorSet decorators) {
  assert((decorators & ON_WEAK_OOP_REF) == 0,
         "nmethod oops have phantom strength, not weak");
  nmethod* nm = CodeCache::find_nmethod((void*)p);
  if (!is_armed(nm)) {
    // If the nmethod entry barrier isn't armed, then it has been applied
    // already. The implication is that the contents of the memory location
    // is already a valid oop, and the barrier would have kept it alive if
    // necessary. Therefore, no action is required, and we are allowed to
    // simply read the oop.
    return *p;
  }

  bool keep_alive = (decorators & ON_PHANTOM_OOP_REF) != 0 &&
                    (decorators & AS_NO_KEEPALIVE) == 0;
  ZTraceThreadEvent event("ZNMethod load_oop");
  ZLocker<ZReentrantLock> locker(ZNMethod::lock_for_nmethod(nm));

  // Make a local root
  zaddress_unsafe obj = *ZUncoloredRoot::cast(p);

  if (keep_alive) {
    ZUncoloredRoot::process(&obj, ZNMethod::color(nm));
  } else {
    ZUncoloredRoot::process_no_keepalive(&obj, ZNMethod::color(nm));
  }

  return to_oop(safe(obj));
}

class ZNMethodUnlinkClosure : public NMethodClosure {
private:
  bool          _unloading_occurred;
  volatile bool _failed;

  void set_failed() {
    Atomic::store(&_failed, true);
  }

  void unlink(nmethod* nm) {
    // Unlinking of the dependencies must happen before the
    // handshake separating unlink and purge.
    nm->flush_dependencies(false /* delete_immediately */);

    // unlink_from_method will take the CompiledMethod_lock.
    // In this case we don't strictly need it when unlinking nmethods from
    // the Method, because it is only concurrently unlinked by
    // the entry barrier, which acquires the per nmethod lock.
    nm->unlink_from_method();

    if (nm->is_osr_method()) {
      // Invalidate the osr nmethod before the handshake. The nmethod
      // will be made unloaded after the handshake. Then invalidate_osr_method()
      // will be called again, which will be a no-op.
      nm->invalidate_osr_method();
    }
  }

public:
  ZNMethodUnlinkClosure(bool unloading_occurred) :
      _unloading_occurred(unloading_occurred),
      _failed(false) {}

  virtual void do_nmethod(nmethod* nm) {
    if (failed()) {
      return;
    }

    if (!nm->is_alive()) {
      return;
    }

    if (nm->is_unloading()) {
      ZTraceThreadEvent event("ZNMethod ZNMethodUnlinkClosure (unloading)");
      ZLocker<ZReentrantLock> locker(ZNMethod::lock_for_nmethod(nm));
      unlink(nm);
      return;
    }

    ZTraceThreadEvent event("ZNMethod ZNMethodUnlinkClosure");
    ZLocker<ZReentrantLock> locker(ZNMethod::lock_for_nmethod(nm));

    if (ZNMethod::is_armed(nm)) {
      // Heal barriers
      ZNMethod::nmethod_patch_barriers(nm);

      ZUncoloredRootProcessNoKeepaliveOopClosure cl(ZNMethod::color(nm));
      ZNMethod::nmethod_oops_do_inner(nm, &cl);

      // Disarm
      ZNMethod::disarm(nm);
    }

    // Clear compiled ICs and exception caches
    if (!nm->unload_nmethod_caches(_unloading_occurred)) {
      set_failed();
    }
  }

  bool failed() const {
    return Atomic::load(&_failed);
  }
};

class ZNMethodUnlinkTask : public ZTask {
private:
  ZNMethodUnlinkClosure _cl;
  ICRefillVerifier*     _verifier;

public:
  ZNMethodUnlinkTask(bool unloading_occurred, ICRefillVerifier* verifier) :
      ZTask("ZNMethodUnlinkTask"),
      _cl(unloading_occurred),
      _verifier(verifier) {
    ZNMethodTable::nmethods_do_begin(false /* secondary */);
  }

  ~ZNMethodUnlinkTask() {
    ZNMethodTable::nmethods_do_end(false /* secondary */);
  }

  virtual void work() {
    ICRefillVerifierMark mark(_verifier);
    ZNMethodTable::nmethods_do(false /* secondary */, &_cl);
  }

  bool success() const {
    return !_cl.failed();
  }
};

void ZNMethod::unlink(ZWorkers* workers, bool unloading_occurred) {
  for (;;) {
    ICRefillVerifier verifier;

    {
      ZNMethodUnlinkTask task(unloading_occurred, &verifier);
      workers->run(&task);
      if (task.success()) {
        return;
      }
    }

    // Cleaning failed because we ran out of transitional IC stubs,
    // so we have to refill and try again. Refilling requires taking
    // a safepoint, so we temporarily leave the suspendible thread set.
    SuspendibleThreadSetLeaver sts;
    InlineCacheBuffer::refill_ic_stubs();
  }
}

class ZNMethodPurgeClosure : public NMethodClosure {
public:
  virtual void do_nmethod(nmethod* nm) {
    if (nm->is_alive() && nm->is_unloading()) {
      nm->make_unloaded();
    }
  }
};

class ZNMethodPurgeTask : public ZTask {
private:
  ZNMethodPurgeClosure _cl;

public:
  ZNMethodPurgeTask() :
      ZTask("ZNMethodPurgeTask"),
      _cl() {
    ZNMethodTable::nmethods_do_begin(false /* secondary */);
  }

  ~ZNMethodPurgeTask() {
    ZNMethodTable::nmethods_do_end(false /* secondary */);
  }

  virtual void work() {
    ZNMethodTable::nmethods_do(false /* secondary */, &_cl);
  }
};

void ZNMethod::purge(ZWorkers* workers) {
  ZNMethodPurgeTask task;
  workers->run(&task);
}
