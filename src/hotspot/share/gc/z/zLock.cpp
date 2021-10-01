/*
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
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
#include "jfr/jfrEvents.hpp"
#include "zLock.inline.hpp"

void ZLockInstrumentation::report(Ticks start, Ticks end) {
  EventZLockContention event;
  event.set_starttime(start);
  event.set_endtime(end);
  event.set_name(_name);
  event.commit();

  log_info(gc)("ZLock contention: %s duration: %.3f ms", _name, ((end - start).seconds() * 1000));
}

ZLock::ZLock(const char* name) :
    _lock(),
    _name(name) {}

ZReentrantLock::ZReentrantLock(const char* name) :
    _lock(name),
    _owner(NULL),
    _count(0) {}

ZConditionLock::ZConditionLock(const char* name) :
    _lock(),
    _name(name) {}

