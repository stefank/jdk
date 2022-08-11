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

#ifndef SHARE_UTILITIES_EVENTS_HPP
#define SHARE_UTILITIES_EVENTS_HPP

#include "memory/allocation.hpp"
#include "runtime/mutex.hpp"
#include "utilities/formatBuffer.hpp"
#include "utilities/globalDefinitions.hpp"

class InstanceKlass;
class outputStream;

// Events and EventMark provide interfaces to log events taking place in the vm.
// This facility is extremely useful for post-mortem debugging. The eventlog
// often provides crucial information about events leading up to the crash.
//
// Abstractly the logs can record whatever they way but normally they
// would record at least a timestamp and the current Thread, along
// with whatever data they need in a ring buffer.  Commonly fixed
// length text messages are recorded for simplicity but other
// strategies could be used.  Several logs are provided by default but
// new instances can be created as needed.

// The base event log dumping class that is registered for dumping at
// crash time.  This is a very generic interface that is mainly here
// for completeness.  Normally the templated EventLogBase would be
// subclassed to provide different log types.
class EventLog : public CHeapObj<mtInternal> {
  friend class Events;

private:
  EventLog* _next;

  EventLog* next() const { return _next; }

public:
  // Automatically registers the log so that it will be printed during
  // crashes.
  EventLog();

  // Print log to output stream.
  virtual void print_log_on(outputStream* out, int max = -1) = 0;

  // Returns true if s matches either the log name or the log handle.
  virtual bool matches_name_or_handle(const char* s) const = 0;

  // Print log names (for help output of VM.events).
  virtual void print_names(outputStream* out) const = 0;
};

// Non-template specific parts of EventLogImpl class.
class EventLogImplBase : public EventLog {
protected:
  Mutex             _mutex;

  // Name is printed out as a header.
  const char* const _name;
  // Handle is a short specifier used to select this particular event log
  // for printing (see VM.events command).
  const char* const _handle;
  const int         _length;
  int               _index;
  int               _count;
  // Typed _records provided by template subclass

  // Returns true if s matches either the log name or the log handle.
  bool matches_name_or_handle(const char* s) const override;

  // Print log names (for help output of VM.events).
  void print_names(outputStream* out) const override;

  void print_log_on_inner(outputStream* out, int max = -1);

  // Print a record - with decomposed parts of a "record".
  void print_record_decomposed_on(outputStream* out, double timestamp, Thread* thread, const char* msg);

  // Print one record - implemented by template subclass
  virtual void print_record_on(outputStream* out, int index) = 0;

public:
  EventLogImplBase(const char* name, const char* handle, int length)
    : _mutex(Mutex::event, name),
      _name(name),
      _handle(handle),
      _length(length),
      _index(0),
      _count(0) {}

  const char* handle() const { return _handle; }
  const char* name()   const { return _name; }

  inline bool should_log();

  // move the ring buffer to next open slot and return the index of
  // the slot to use for the current message.  Should only be called
  // while mutex is held.
  inline int compute_log_index();

  inline double fetch_timestamp();

  // Print the contents of the log
  void print_log_on(outputStream* out, int max = -1) override;
};

// A templated subclass of EventLog that provides basic ring buffer
// functionality.  Most event loggers should subclass this, possibly
// providing a more featureful log function if the existing copy
// semantics aren't appropriate.  The name is used as the label of the
// log when it is dumped during a crash.
//
// The non-template specific parts have been split out into a base
// class to make it easier to put implementation in the cpp file.
template <class T> class EventLogImpl : public EventLogImplBase {
  template <class X> class EventRecord : public CHeapObj<mtInternal> {
   public:
    double  timestamp;
    Thread* thread;
    X       data;
  };

 protected:
  EventRecord<T>* _records;

 public:
  EventLogImpl<T>(const char* name, const char* handle, int length = LogEventsBufferEntries)
    : EventLogImplBase(name, handle, length),
      _records(new EventRecord<T>[length]) {}

private:
  void print_record_on(outputStream* out, int index) override {
    // Decompose the record, and print it
    print_record_decomposed_on(out,
                               _records[index].timestamp,
                               _records[index].thread,
                               _records[index].data);
  }
};

// A simple wrapper class for fixed size text messages.
template <size_t bufsz>
class FormatStringLogMessage : public FormatBuffer<bufsz> {};
typedef FormatStringLogMessage<256> StringLogMessage;

// A simple ring buffer of fixed size text messages.
template <size_t bufsz>
class FormatStringEventLog : public EventLogImpl< FormatStringLogMessage<bufsz> > {
public:
  FormatStringEventLog(const char* name, const char* short_name, int count = LogEventsBufferEntries)
    : EventLogImpl< FormatStringLogMessage<bufsz> >(name, short_name, count) {}

  inline void logv(Thread* thread, const char* format, va_list ap) ATTRIBUTE_PRINTF(3, 0);
  inline void log(Thread* thread, const char* format, ...) ATTRIBUTE_PRINTF(3, 4);
};

typedef FormatStringEventLog<256>  StringEventLog;
typedef FormatStringEventLog<512>  ExtendedStringEventLog;
typedef FormatStringEventLog<1024> ExtraExtendedStringEventLog;

// Event log for class unloading events to materialize the class name in place in the log stream.
class UnloadingEventLog : public StringEventLog {
public:
  UnloadingEventLog(const char* name, const char* short_name, int count = LogEventsBufferEntries)
    : StringEventLog(name, short_name, count) {}

  void log(Thread* thread, InstanceKlass* ik);
};

// Event log for exceptions
class ExceptionsEventLog : public ExtendedStringEventLog {
public:
  ExceptionsEventLog(const char* name, const char* short_name, int count = LogEventsBufferEntries)
    : ExtendedStringEventLog(name, short_name, count) {}

  void log(Thread* thread, Handle h_exception, const char* message, const char* file, int line);
};


class Events : AllStatic {
  friend class EventLog;

private:
  static EventLog* _logs;

  // A log for generic messages that aren't well categorized.
  static StringEventLog* _messages;

  // A log for VM Operations
  static StringEventLog* _vm_operations;

  // A log for internal exception related messages, like internal
  // throws and implicit exceptions.
  static ExceptionsEventLog* _exceptions;

  // Deoptization related messages
  static StringEventLog* _deopt_messages;

  // dynamic lib related messages
  static StringEventLog* _dll_messages;

  // Redefinition related messages
  static StringEventLog* _redefinitions;

  // Class unloading events
  static UnloadingEventLog* _class_unloading;

  // Class loading events
  static StringEventLog* _class_loading;
 public:

  // Print all event logs; limit number of events per event log to be printed with max
  // (max == -1 prints all events).
  static void print_all(outputStream* out, int max = -1);

  // Print a single event log specified by name or handle.
  static void print_one(outputStream* out, const char* log_name, int max = -1);

  // Dump all events to the tty
  static void print();

  // Logs a generic message with timestamp and format as printf.
  inline static void log(Thread* thread, const char* format, ...) ATTRIBUTE_PRINTF(2, 3);

  inline static void log_vm_operation(Thread* thread, const char* format, ...) ATTRIBUTE_PRINTF(2, 3);

  // Log exception related message
  inline static void log_exception(Thread* thread, const char* format, ...) ATTRIBUTE_PRINTF(2, 3);
  inline static void log_exception(Thread* thread, Handle h_exception, const char* message, const char* file, int line);

  inline static void log_redefinition(Thread* thread, const char* format, ...) ATTRIBUTE_PRINTF(2, 3);

  inline static void log_class_unloading(Thread* thread, InstanceKlass* ik);

  inline static void log_class_loading(Thread* thread, const char* format, ...) ATTRIBUTE_PRINTF(2, 3);

  inline static void log_deopt_message(Thread* thread, const char* format, ...) ATTRIBUTE_PRINTF(2, 3);

  inline static void log_dll_message(Thread* thread, const char* format, ...) ATTRIBUTE_PRINTF(2, 3);

  // Register default loggers
  static void init();
};

typedef void (*EventLogFunction)(Thread* thread, const char* format, ...);

class EventMarkBase : public StackObj {
  EventLogFunction _log_function;
  StringLogMessage _buffer;

  NONCOPYABLE(EventMarkBase);

protected:
  void log_start(const char* format, va_list argp) ATTRIBUTE_PRINTF(2, 0);
  void log_end();

  EventMarkBase(EventLogFunction log_function);
};

// Place markers for the beginning and end up of a set of events.
template <EventLogFunction log_function>
class EventMarkWithLogFunction : public EventMarkBase {
  StringLogMessage _buffer;

public:
  // log a begin event, format as printf
  EventMarkWithLogFunction(const char* format, ...) ATTRIBUTE_PRINTF(2, 3)
    : EventMarkBase(log_function) {
    if (LogEvents) {
      va_list ap;
      va_start(ap, format);
      log_start(format, ap);
      va_end(ap);
    }
  }
  // log an end event
  ~EventMarkWithLogFunction() {
    if (LogEvents) {
      log_end();
    }
  }
};

#endif // SHARE_UTILITIES_EVENTS_HPP
