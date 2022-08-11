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

#ifndef SHARE_UTILITIES_EVENTS_INLINE_HPP
#define SHARE_UTILITIES_EVENTS_INLINE_HPP

#include "utilities/events.hpp"

#include "runtime/globals.hpp"
#include "runtime/mutexLocker.hpp"
#include "runtime/os.hpp"
#include "utilities/ostream.hpp"
#include "utilities/vmError.hpp"

inline bool EventLogImplBase::should_log() {
  // Don't bother adding new entries when we're crashing.  This also
  // avoids mutating the ring buffer when printing the log.
  return !VMError::is_error_reported();
}

int EventLogImplBase::compute_log_index() {
  int index = _index;
  if (_count < _length) {
    _count++;
  }
  _index++;
  if (_index >= _length) {
    _index = 0;
  }
  return index;
}

inline double EventLogImplBase::fetch_timestamp() {
  return os::elapsedTime();
}

template <size_t bufsz>
void FormatStringEventLog<bufsz>::logv(Thread* thread, const char* format, va_list ap) {
  if (!this->should_log()) {
    return;
  }

  double timestamp = this->fetch_timestamp();
  MutexLocker ml(&this->_mutex, Mutex::_no_safepoint_check_flag);
  int index = this->compute_log_index();
  this->_records[index].thread = thread;
  this->_records[index].timestamp = timestamp;
  this->_records[index].data.printv(format, ap);
}

template <size_t bufsz>
void FormatStringEventLog<bufsz>::log(Thread* thread, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  this->logv(thread, format, ap);
  va_end(ap);
}

inline void Events::log(Thread* thread, const char* format, ...) {
  if (LogEvents && _messages != NULL) {
    va_list ap;
    va_start(ap, format);
    _messages->logv(thread, format, ap);
    va_end(ap);
  }
}

inline void Events::log_vm_operation(Thread* thread, const char* format, ...) {
  if (LogEvents && _vm_operations != NULL) {
    va_list ap;
    va_start(ap, format);
    _vm_operations->logv(thread, format, ap);
    va_end(ap);
  }
}

inline void Events::log_exception(Thread* thread, const char* format, ...) {
  if (LogEvents && _exceptions != NULL) {
    va_list ap;
    va_start(ap, format);
    _exceptions->logv(thread, format, ap);
    va_end(ap);
  }
}

inline void Events::log_exception(Thread* thread, Handle h_exception, const char* message, const char* file, int line) {
  if (LogEvents && _exceptions != NULL) {
    _exceptions->log(thread, h_exception, message, file, line);
  }
}

inline void Events::log_redefinition(Thread* thread, const char* format, ...) {
  if (LogEvents && _redefinitions != NULL) {
    va_list ap;
    va_start(ap, format);
    _redefinitions->logv(thread, format, ap);
    va_end(ap);
  }
}

inline void Events::log_class_unloading(Thread* thread, InstanceKlass* ik) {
  if (LogEvents && _class_unloading != NULL) {
    _class_unloading->log(thread, ik);
  }
}

inline void Events::log_class_loading(Thread* thread, const char* format, ...) {
  if (LogEvents && _class_loading != NULL) {
    va_list ap;
    va_start(ap, format);
    _class_loading->logv(thread, format, ap);
    va_end(ap);
  }
}

inline void Events::log_deopt_message(Thread* thread, const char* format, ...) {
  if (LogEvents && _deopt_messages != NULL) {
    va_list ap;
    va_start(ap, format);
    _deopt_messages->logv(thread, format, ap);
    va_end(ap);
  }
}

inline void Events::log_dll_message(Thread* thread, const char* format, ...) {
  if (LogEvents && _dll_messages != NULL) {
    va_list ap;
    va_start(ap, format);
    _dll_messages->logv(thread, format, ap);
    va_end(ap);
  }
}

// These end up in the default log.
typedef EventMarkWithLogFunction<Events::log> EventMark;

// These end up in the vm_operation log.
typedef EventMarkWithLogFunction<Events::log_vm_operation> EventMarkVMOperation;

// These end up in the class loading log.
typedef EventMarkWithLogFunction<Events::log_class_loading> EventMarkClassLoading;

#endif // SHARE_UTILITIES_EVENTS_INLINE_HPP
