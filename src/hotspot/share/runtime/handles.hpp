/*
 * Copyright (c) 1997, 2021, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_RUNTIME_HANDLES_HPP
#define SHARE_RUNTIME_HANDLES_HPP

#include "memory/arena.hpp"
#include "oops/oop.hpp"
#include "oops/oopsHierarchy.hpp"

class HandleList;
class InstanceKlass;
class Klass;
class Thread;

//------------------------------------------------------------------------------------------------------------------------
// In order to preserve oops during garbage collection, they should be
// allocated and passed around via Handles within the VM. A handle is
// simply an extra indirection.
//
// oop parameters and return types should be Handles whenever feasible.
//
// Handles are declared in a straight-forward manner, e.g.
//
//   oop obj = ...;
//   Handle h2(thread, obj);      // create a new handle and link it into the thread
//   Handle h3;                   // declare handle only, no linking occurs
//   ...
//   h3 = h1;                     // make h3 refer to same oop as h1, link h3 into the thread
//   oop obj2 = h2();             // get handle value
//   h1->print();                 // invoking operation on oop
//
// Handles are specialized for different oop types to provide extra type
// information and avoid unnecessary casting. For each oop type xxxOop
// there is a corresponding handle called xxxHandle.

//------------------------------------------------------------------------------------------------------------------------
// Base class for all handles. Provides overloading of frequently
// used operators for ease of use.

class Handle {
  friend class HandleList;

 private:
  // Use oopDesc* instead of oop to navigate around CheckUnhandledOops.
  oopDesc* _obj;

  // Active handles are linked in lists that belong to the thread.
  // The list is double linked to enable fast unlinking.
  Handle*  _next;
  Handle*  _prev;

  void unlink();

  void verify_links() const NOT_DEBUG_RETURN;

 protected:
  oop     obj() const                            { verify_links(); return _obj; }
  oop     non_null_obj() const                   { assert(_obj != NULL, "resolving NULL handle"); return obj(); }

  Handle(oop obj, Handle* next, Handle* prev) :
      _obj(obj), _next(next), _prev(prev) {}

 public:
  // Constructors
  Handle() : Handle(NULL, NULL, NULL) {}
  Handle(Thread* thread, oop obj);
  Handle(const Handle& other);

  ~Handle();

  Handle& operator=(const Handle& other);

  // General access
  oop     operator () () const                   { return obj(); }
  oop     operator -> () const                   { return non_null_obj(); }

  bool operator == (oop o) const                 { return obj() == o; }
  bool operator != (oop o) const                 { return obj() != o; }
  bool operator == (const Handle& h) const       { return obj() == h.obj(); }
  bool operator != (const Handle& h) const       { return obj() != h.obj(); }

  // Null checks
  bool    is_null() const                        { return _obj == NULL; }
  bool    not_null() const                       { return _obj != NULL; }

  // Debugging
  void    print()                                { obj()->print(); }

  // Raw handle access. Allows easy duplication of Handles. This can be very unsafe
  // since duplicates is only valid as long as original handle is alive.
  oop* raw_value() const                         { return (oop*)&_obj; }
};

class HandleList {
private:
  Handle _head;

  bool is_empty() const;

  void verify_linked(const Handle* handle) const NOT_DEBUG_RETURN;
  void verify_head() const NOT_DEBUG_RETURN;

  void link(Handle* handle);
  void clear();

public:
  HandleList();
  HandleList(const HandleList&) = delete;
  HandleList(HandleList&& other) = delete;

  HandleList& operator=(const HandleList&) = delete;
  HandleList& operator=(HandleList&& other) = delete;

  ~HandleList();

  void add(Handle* handle);

  void clear_handles();

  void oops_do(OopClosure* cl) const;

  bool is_in(const Handle* handle) const;

  static HandleList* handle_list_for(const Handle* handle);
};

// Specific Handles for different oop types
#define DEF_HANDLE(type, is_a)                   \
  class type##Handle: public Handle {            \
   protected:                                    \
    type##Oop    obj() const                     { return (type##Oop)Handle::obj(); } \
    type##Oop    non_null_obj() const            { return (type##Oop)Handle::non_null_obj(); } \
                                                 \
   public:                                       \
    /* Constructors */                           \
    type##Handle ()                              : Handle()                 {} \
    inline type##Handle (Thread* thread, type##Oop obj); \
    \
    /* Operators for ease of use */              \
    type##Oop    operator () () const            { return obj(); } \
    type##Oop    operator -> () const            { return non_null_obj(); } \
  };


DEF_HANDLE(instance         , is_instance_noinline         )
DEF_HANDLE(array            , is_array_noinline            )
DEF_HANDLE(objArray         , is_objArray_noinline         )
DEF_HANDLE(typeArray        , is_typeArray_noinline        )

//------------------------------------------------------------------------------------------------------------------------

// Metadata Handles.  Unlike oop Handles these are needed to prevent metadata
// from being reclaimed by RedefineClasses.
// Metadata Handles should be passed around as const references to avoid copy construction
// and destruction for parameters.

// Specific Handles for different oop types
#define DEF_METADATA_HANDLE(name, type)          \
  class name##Handle;                            \
  class name##Handle : public StackObj {         \
    type*     _value;                            \
    Thread*   _thread;                           \
   protected:                                    \
    type*        obj() const                     { return _value; } \
    type*        non_null_obj() const            { assert(_value != NULL, "resolving NULL _value"); return _value; } \
                                                 \
   public:                                       \
    /* Constructors */                           \
    name##Handle () : _value(NULL), _thread(NULL) {}   \
    name##Handle (Thread* thread, type* obj);    \
                                                 \
    name##Handle (const name##Handle &h);        \
    name##Handle& operator=(const name##Handle &s); \
                                                 \
    /* Destructor */                             \
    ~name##Handle ();                            \
    void remove();                               \
                                                 \
    /* Operators for ease of use */              \
    type*        operator () () const            { return obj(); } \
    type*        operator -> () const            { return non_null_obj(); } \
                                                 \
    bool    operator == (type* o) const          { return obj() == o; } \
    bool    operator == (const name##Handle& h) const  { return obj() == h.obj(); } \
                                                 \
    /* Null checks */                            \
    bool    is_null() const                      { return _value == NULL; } \
    bool    not_null() const                     { return _value != NULL; } \
  };


DEF_METADATA_HANDLE(method, Method)
DEF_METADATA_HANDLE(constantPool, ConstantPool)

//------------------------------------------------------------------------------------------------------------------------
// Thread local handle area
class HandleArea: public Arena {
  friend class HandleMark;
  friend class NoHandleMark;
  friend class ResetNoHandleMark;
#ifdef ASSERT
  int _handle_mark_nesting;
  int _no_handle_mark_nesting;
#endif
  HandleArea* _prev;          // link to outer (older) area
 public:
  // Constructor
  HandleArea(HandleArea* prev) : Arena(mtThread, Chunk::tiny_size) {
    debug_only(_handle_mark_nesting    = 0);
    debug_only(_no_handle_mark_nesting = 0);
    _prev = prev;
  }

  // Handle allocation
 private:
  oop* real_allocate_handle(oop obj) {
    // Ignore UseMallocOnly by allocating only in arena.
    oop* handle = (oop*)internal_amalloc(oopSize);
    *handle = obj;
    return handle;
  }
 public:
#ifdef ASSERT
  oop* allocate_handle(oop obj);
#else
  oop* allocate_handle(oop obj) { return real_allocate_handle(obj); }
#endif

  // Garbage collection support
  void oops_do(OopClosure* f);

  // Number of handles in use
  size_t used() const     { return Arena::used() / oopSize; }

  debug_only(bool no_handle_mark_active() { return _no_handle_mark_nesting > 0; })
};


//------------------------------------------------------------------------------------------------------------------------
// Handles are allocated in a (growable) thread local handle area. Deallocation
// is managed using a HandleMark. It should normally not be necessary to use
// HandleMarks manually.
//
// A HandleMark constructor will record the current handle area top, and the
// destructor will reset the top, destroying all handles allocated in between.
// The following code will therefore NOT work:
//
//   Handle h;
//   {
//     HandleMark hm(THREAD);
//     h = Handle(THREAD, obj);
//   }
//   h()->print();       // WRONG, h destroyed by HandleMark destructor.
//
// If h has to be preserved, it can be converted to an oop or a local JNI handle
// across the HandleMark boundary.

// The base class of HandleMark should have been StackObj but we also heap allocate
// a HandleMark when a thread is created. The operator new is for this special case.

class HandleMark {
 private:
  Thread *_thread;              // thread that owns this mark
  HandleArea *_area;            // saved handle area
  Chunk *_chunk;                // saved arena chunk
  char *_hwm, *_max;            // saved arena info
  size_t _size_in_bytes;        // size of handle area
  // Link to previous active HandleMark in thread
  HandleMark* _previous_handle_mark;

  void initialize(Thread* thread);                // common code for constructors
  void set_previous_handle_mark(HandleMark* mark) { _previous_handle_mark = mark; }
  HandleMark* previous_handle_mark() const        { return _previous_handle_mark; }

  size_t size_in_bytes() const { return _size_in_bytes; }
  // remove all chunks beginning with the next
  void chop_later_chunks();
 public:
  HandleMark(Thread* thread)                      { initialize(thread); }
  ~HandleMark();

  // Functions used by HandleMarkCleaner
  // called in the constructor of HandleMarkCleaner
  void push();
  // called in the destructor of HandleMarkCleaner
  void pop_and_restore();
  // overloaded operators
  void* operator new(size_t size) throw();
  void* operator new [](size_t size) throw();
  void operator delete(void* p);
  void operator delete[](void* p);
};

//------------------------------------------------------------------------------------------------------------------------
// A NoHandleMark stack object will verify that no handles are allocated
// in its scope. Enabled in debug mode only.

class NoHandleMark: public StackObj {
 public:
#ifdef ASSERT
  NoHandleMark();
  ~NoHandleMark();
#else
  NoHandleMark()  {}
  ~NoHandleMark() {}
#endif
};


// ResetNoHandleMark is called in a context where there is an enclosing
// NoHandleMark. A thread in _thread_in_native must not create handles so
// this is used when transitioning via ThreadInVMfromNative.
class ResetNoHandleMark: public StackObj {
  int _no_handle_mark_nesting;
 public:
#ifdef ASSERT
  ResetNoHandleMark();
  ~ResetNoHandleMark();
#else
  ResetNoHandleMark()  {}
  ~ResetNoHandleMark() {}
#endif
};

// The HandleMarkCleaner is a faster version of HandleMark.
// It relies on the fact that there is a HandleMark further
// down the stack (in JavaCalls::call_helper), and just resets
// to the saved values in that HandleMark.

class HandleMarkCleaner: public StackObj {
 private:
  Thread* _thread;
 public:
  inline HandleMarkCleaner(Thread* thread);
  inline ~HandleMarkCleaner();
};

#endif // SHARE_RUNTIME_HANDLES_HPP
