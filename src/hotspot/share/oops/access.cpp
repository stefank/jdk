#include "precompiled.hpp"
#include "oops/access.inline.hpp"
#include "oops/accessBackend.inline.hpp"
#include "gc/shared/barrierSetConfig.inline.hpp"
#include "gc/shared/barrierSet.inline.hpp"

namespace AccessInternal {

  template <class GCBarrierType, DecoratorSet decorators>
  template <typename T>
  void PostRuntimeDispatch<GCBarrierType, BARRIER_STORE, decorators>::access_barrier(void* addr, T value) {
    GCBarrierType::store_in_heap(reinterpret_cast<T*>(addr), value);
  }

  template <class GCBarrierType, DecoratorSet decorators>
  void PostRuntimeDispatch<GCBarrierType, BARRIER_STORE, decorators>::oop_access_barrier(void* addr, oop value) {
    typedef typename HeapOopType<decorators>::type OopType;
    if (HasDecorator<decorators, IN_HEAP>::value) {
      GCBarrierType::oop_store_in_heap(reinterpret_cast<OopType*>(addr), value);
    } else {
      GCBarrierType::oop_store_not_in_heap(reinterpret_cast<OopType*>(addr), value);
    }
  }

  template <class GCBarrierType, DecoratorSet decorators>
  template <typename T>
  T PostRuntimeDispatch<GCBarrierType, BARRIER_LOAD, decorators>::access_barrier(void* addr) {
    return GCBarrierType::load_in_heap(reinterpret_cast<T*>(addr));
  }

  template <class GCBarrierType, DecoratorSet decorators>
  oop PostRuntimeDispatch<GCBarrierType, BARRIER_LOAD, decorators>::oop_access_barrier(void* addr) {
    typedef typename HeapOopType<decorators>::type OopType;
    if (HasDecorator<decorators, IN_HEAP>::value) {
      return GCBarrierType::oop_load_in_heap(reinterpret_cast<OopType*>(addr));
    } else {
      return GCBarrierType::oop_load_not_in_heap(reinterpret_cast<OopType*>(addr));
    }
  }

  template <class GCBarrierType, DecoratorSet decorators>
  template <typename T>
  T PostRuntimeDispatch<GCBarrierType, BARRIER_ATOMIC_XCHG, decorators>::access_barrier(void* addr, T new_value) {
    return GCBarrierType::atomic_xchg_in_heap(reinterpret_cast<T*>(addr), new_value);
  }

  template <class GCBarrierType, DecoratorSet decorators>
  oop PostRuntimeDispatch<GCBarrierType, BARRIER_ATOMIC_XCHG, decorators>::oop_access_barrier(void* addr, oop new_value) {
    typedef typename HeapOopType<decorators>::type OopType;
    if (HasDecorator<decorators, IN_HEAP>::value) {
      return GCBarrierType::oop_atomic_xchg_in_heap(reinterpret_cast<OopType*>(addr), new_value);
    } else {
      return GCBarrierType::oop_atomic_xchg_not_in_heap(reinterpret_cast<OopType*>(addr), new_value);
    }
  }

  template <class GCBarrierType, DecoratorSet decorators>
  template <typename T>
  T PostRuntimeDispatch<GCBarrierType, BARRIER_ATOMIC_CMPXCHG, decorators>::access_barrier(void* addr, T compare_value, T new_value) {
    return GCBarrierType::atomic_cmpxchg_in_heap(reinterpret_cast<T*>(addr), compare_value, new_value);
  }

  template <class GCBarrierType, DecoratorSet decorators>
  oop PostRuntimeDispatch<GCBarrierType, BARRIER_ATOMIC_CMPXCHG, decorators>::oop_access_barrier(void* addr, oop compare_value, oop new_value) {
    typedef typename HeapOopType<decorators>::type OopType;
    if (HasDecorator<decorators, IN_HEAP>::value) {
      return GCBarrierType::oop_atomic_cmpxchg_in_heap(reinterpret_cast<OopType*>(addr), compare_value, new_value);
    } else {
      return GCBarrierType::oop_atomic_cmpxchg_not_in_heap(reinterpret_cast<OopType*>(addr), compare_value, new_value);
    }
  }

  template <class GCBarrierType, DecoratorSet decorators>
  template <typename T>
  bool PostRuntimeDispatch<GCBarrierType, BARRIER_ARRAYCOPY, decorators>::access_barrier(arrayOop src_obj, size_t src_offset_in_bytes, T* src_raw,
                                                                                         arrayOop dst_obj, size_t dst_offset_in_bytes, T* dst_raw,
                                                                                         size_t length) {
    GCBarrierType::arraycopy_in_heap(src_obj, src_offset_in_bytes, src_raw,
                                     dst_obj, dst_offset_in_bytes, dst_raw,
                                     length);
    return true;
  }

  template <class GCBarrierType, DecoratorSet decorators>
  template <typename T>
  bool PostRuntimeDispatch<GCBarrierType, BARRIER_ARRAYCOPY, decorators>::oop_access_barrier(arrayOop src_obj, size_t src_offset_in_bytes, T* src_raw,
                                                                                             arrayOop dst_obj, size_t dst_offset_in_bytes, T* dst_raw,
                                                                                             size_t length) {
    typedef typename HeapOopType<decorators>::type OopType;
    return GCBarrierType::oop_arraycopy_in_heap(src_obj, src_offset_in_bytes, reinterpret_cast<OopType*>(src_raw),
                                                dst_obj, dst_offset_in_bytes, reinterpret_cast<OopType*>(dst_raw),
                                                length);
  }

  template <class GCBarrierType, DecoratorSet decorators>
  template <typename T>
  void PostRuntimeDispatch<GCBarrierType, BARRIER_STORE_AT, decorators>::access_barrier(oop base, ptrdiff_t offset, T value) {
    GCBarrierType::store_in_heap_at(base, offset, value);
  }

  template <class GCBarrierType, DecoratorSet decorators>
  void PostRuntimeDispatch<GCBarrierType, BARRIER_STORE_AT, decorators>::oop_access_barrier(oop base, ptrdiff_t offset, oop value) {
    GCBarrierType::oop_store_in_heap_at(base, offset, value);
  }

  template <class GCBarrierType, DecoratorSet decorators>
  template <typename T>
  T PostRuntimeDispatch<GCBarrierType, BARRIER_LOAD_AT, decorators>::access_barrier(oop base, ptrdiff_t offset) {
    return GCBarrierType::template load_in_heap_at<T>(base, offset);
  }

  template <class GCBarrierType, DecoratorSet decorators>
  oop PostRuntimeDispatch<GCBarrierType, BARRIER_LOAD_AT, decorators>::oop_access_barrier(oop base, ptrdiff_t offset) {
    return GCBarrierType::oop_load_in_heap_at(base, offset);
  }

  template <class GCBarrierType, DecoratorSet decorators>
  template <typename T>
  T PostRuntimeDispatch<GCBarrierType, BARRIER_ATOMIC_XCHG_AT, decorators>::access_barrier(oop base, ptrdiff_t offset, T new_value) {
    return GCBarrierType::atomic_xchg_in_heap_at(base, offset, new_value);
  }

  template <class GCBarrierType, DecoratorSet decorators>
  oop PostRuntimeDispatch<GCBarrierType, BARRIER_ATOMIC_XCHG_AT, decorators>::oop_access_barrier(oop base, ptrdiff_t offset, oop new_value) {
    return GCBarrierType::oop_atomic_xchg_in_heap_at(base, offset, new_value);
  }


  template <class GCBarrierType, DecoratorSet decorators>
  template <typename T>
  T PostRuntimeDispatch<GCBarrierType, BARRIER_ATOMIC_CMPXCHG_AT, decorators>::access_barrier(oop base, ptrdiff_t offset, T compare_value, T new_value) {
    return GCBarrierType::atomic_cmpxchg_in_heap_at(base, offset, compare_value, new_value);
  }

  template <class GCBarrierType, DecoratorSet decorators>
  oop PostRuntimeDispatch<GCBarrierType, BARRIER_ATOMIC_CMPXCHG_AT, decorators>::oop_access_barrier(oop base, ptrdiff_t offset, oop compare_value, oop new_value) {
    return GCBarrierType::oop_atomic_cmpxchg_in_heap_at(base, offset, compare_value, new_value);
  }

  template <class GCBarrierType, DecoratorSet decorators>
  void PostRuntimeDispatch<GCBarrierType, BARRIER_CLONE, decorators>::access_barrier(oop src, oop dst, size_t size) {
    GCBarrierType::clone_in_heap(src, dst, size);
  }

  template <DecoratorSet decorators, typename FunctionPointerT, BarrierType barrier_type>
  template <DecoratorSet ds>
  typename EnableIf<
    HasDecorator<ds, INTERNAL_VALUE_IS_OOP>::value,
    FunctionPointerT>::type
  BarrierResolver<decorators, FunctionPointerT, barrier_type>::resolve_barrier_gc() {
    BarrierSet* bs = BarrierSet::barrier_set();
    assert(bs != NULL, "GC barriers invoked before BarrierSet is set");
    switch (bs->kind()) {
#define BARRIER_SET_RESOLVE_BARRIER_CLOSURE(bs_name)                    \
		case BarrierSet::bs_name: {                                     \
			return PostRuntimeDispatch<typename BarrierSet::GetType<BarrierSet::bs_name>::type:: \
					AccessBarrier<ds>, barrier_type, ds>::oop_access_barrier; \
		}                                                               \
		break;
    FOR_EACH_CONCRETE_BARRIER_SET_DO(BARRIER_SET_RESOLVE_BARRIER_CLOSURE)
#undef BARRIER_SET_RESOLVE_BARRIER_CLOSURE

    default:
      fatal("BarrierSet AccessBarrier resolving not implemented");
      return NULL;
    };
  }

  template <DecoratorSet decorators, typename FunctionPointerT, BarrierType barrier_type>
  template <DecoratorSet ds>
  typename EnableIf<
    !HasDecorator<ds, INTERNAL_VALUE_IS_OOP>::value,
    FunctionPointerT>::type
  BarrierResolver<decorators, FunctionPointerT, barrier_type>::resolve_barrier_gc() {
    BarrierSet* bs = BarrierSet::barrier_set();
    assert(bs != NULL, "GC barriers invoked before BarrierSet is set");
    switch (bs->kind()) {
#define BARRIER_SET_RESOLVE_BARRIER_CLOSURE(bs_name)                    \
      case BarrierSet::bs_name: {                                       \
        return PostRuntimeDispatch<typename BarrierSet::GetType<BarrierSet::bs_name>::type:: \
          AccessBarrier<ds>, barrier_type, ds>::access_barrier; \
      }                                                                 \
      break;
      FOR_EACH_CONCRETE_BARRIER_SET_DO(BARRIER_SET_RESOLVE_BARRIER_CLOSURE)
#undef BARRIER_SET_RESOLVE_BARRIER_CLOSURE

    default:
      fatal("BarrierSet AccessBarrier resolving not implemented");
      return NULL;
    };
  }
// --------------------------------

#define instantiate(name, type, decorators) \
  template struct PostRuntimeDispatch<name ## BarrierSet::AccessBarrier<decorators, name ## BarrierSet>, type, decorators>

  /*
  enum BarrierType {
    BARRIER_STORE,
    BARRIER_STORE_AT,
    BARRIER_LOAD,
    BARRIER_LOAD_AT,
    BARRIER_ATOMIC_CMPXCHG,
    BARRIER_ATOMIC_CMPXCHG_AT,
    BARRIER_ATOMIC_XCHG,
    BARRIER_ATOMIC_XCHG_AT,
    BARRIER_ARRAYCOPY,
    BARRIER_CLONE
  };
  */

#define instantiate2(name) \
  instantiate(name, BARRIER_STORE, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|MO_UNORDERED|AS_NO_KEEPALIVE|ON_STRONG_OOP_REF|IN_HEAP); \
  instantiate(name, BARRIER_STORE, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NORMAL|ON_STRONG_OOP_REF|IN_HEAP); \
  instantiate(name, BARRIER_STORE, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NO_KEEPALIVE|ON_STRONG_OOP_REF|IN_HEAP); \
  instantiate(name, BARRIER_STORE, INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NORMAL|ON_STRONG_OOP_REF|IN_NATIVE); \
  instantiate(name, BARRIER_STORE, INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NORMAL|ON_STRONG_OOP_REF|IN_NATIVE|IS_DEST_UNINITIALIZED); \
  instantiate(name, BARRIER_STORE, INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NORMAL|ON_STRONG_OOP_REF|IN_HEAP); \
  instantiate(name, BARRIER_STORE, INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NORMAL|ON_PHANTOM_OOP_REF|IN_NATIVE); \
  instantiate(name, BARRIER_STORE, INTERNAL_VALUE_IS_OOP|MO_UNORDERED|AS_NORMAL|ON_STRONG_OOP_REF|IN_NATIVE); \
  instantiate(name, BARRIER_STORE, INTERNAL_VALUE_IS_OOP|MO_UNORDERED|AS_NORMAL|ON_STRONG_OOP_REF|IN_NATIVE|IS_DEST_UNINITIALIZED); \
  instantiate(name, BARRIER_STORE, INTERNAL_VALUE_IS_OOP|MO_UNORDERED|AS_NORMAL|ON_STRONG_OOP_REF|IN_HEAP);\
  instantiate(name, BARRIER_STORE, INTERNAL_VALUE_IS_OOP|MO_UNORDERED|AS_NORMAL|ON_PHANTOM_OOP_REF|IN_NATIVE); \
 \
 instantiate(name, BARRIER_LOAD, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NORMAL|ON_STRONG_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_LOAD, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NO_KEEPALIVE|ON_STRONG_OOP_REF|IN_NATIVE);\
 instantiate(name, BARRIER_LOAD, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NO_KEEPALIVE|ON_STRONG_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_LOAD, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NO_KEEPALIVE|ON_PHANTOM_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_LOAD, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NO_KEEPALIVE|ON_WEAK_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_LOAD, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|MO_UNORDERED|AS_NO_KEEPALIVE|ON_PHANTOM_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_LOAD, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|MO_UNORDERED|AS_NO_KEEPALIVE|ON_WEAK_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_LOAD, INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NORMAL|ON_STRONG_OOP_REF|IN_NATIVE);\
 instantiate(name, BARRIER_LOAD, INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NORMAL|ON_STRONG_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_LOAD, INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NORMAL|ON_PHANTOM_OOP_REF|IN_NATIVE);\
 instantiate(name, BARRIER_LOAD, INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NO_KEEPALIVE|ON_STRONG_OOP_REF|IN_NATIVE);\
 instantiate(name, BARRIER_LOAD, INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NO_KEEPALIVE|ON_STRONG_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_LOAD, INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NO_KEEPALIVE|ON_PHANTOM_OOP_REF|IN_NATIVE);\
 instantiate(name, BARRIER_LOAD, INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NO_KEEPALIVE|ON_PHANTOM_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_LOAD, INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NO_KEEPALIVE|ON_WEAK_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_LOAD, INTERNAL_VALUE_IS_OOP|MO_UNORDERED|AS_NORMAL|ON_STRONG_OOP_REF|IN_NATIVE);\
 instantiate(name, BARRIER_LOAD, INTERNAL_VALUE_IS_OOP|MO_UNORDERED|AS_NORMAL|ON_STRONG_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_LOAD, INTERNAL_VALUE_IS_OOP|MO_UNORDERED|AS_NORMAL|ON_PHANTOM_OOP_REF|IN_NATIVE);\
 instantiate(name, BARRIER_LOAD, INTERNAL_VALUE_IS_OOP|MO_UNORDERED|AS_NO_KEEPALIVE|ON_STRONG_OOP_REF|IN_NATIVE);\
 instantiate(name, BARRIER_LOAD, INTERNAL_VALUE_IS_OOP|MO_UNORDERED|AS_NO_KEEPALIVE|ON_STRONG_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_LOAD, INTERNAL_VALUE_IS_OOP|MO_UNORDERED|AS_NO_KEEPALIVE|ON_PHANTOM_OOP_REF|IN_NATIVE);\
 instantiate(name, BARRIER_LOAD, INTERNAL_VALUE_IS_OOP|MO_UNORDERED|AS_NO_KEEPALIVE|ON_PHANTOM_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_LOAD, INTERNAL_VALUE_IS_OOP|MO_UNORDERED|AS_NO_KEEPALIVE|ON_WEAK_OOP_REF|IN_HEAP);\
 \
 instantiate(name, BARRIER_ATOMIC_XCHG, INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_SEQ_CST|AS_NORMAL|ON_STRONG_OOP_REF|IN_NATIVE);\
 instantiate(name, BARRIER_ATOMIC_XCHG, INTERNAL_VALUE_IS_OOP|MO_SEQ_CST|AS_NORMAL|ON_STRONG_OOP_REF|IN_NATIVE);\
 \
 instantiate(name, BARRIER_ATOMIC_CMPXCHG, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_SEQ_CST|AS_NO_KEEPALIVE|ON_STRONG_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_ATOMIC_CMPXCHG, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|MO_SEQ_CST|AS_NO_KEEPALIVE|ON_STRONG_OOP_REF|IN_HEAP);\
 \
 instantiate(name, BARRIER_ARRAYCOPY, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NORMAL|ON_STRONG_OOP_REF|IN_HEAP|IS_ARRAY|IS_DEST_UNINITIALIZED|ARRAYCOPY_ARRAYOF);\
 instantiate(name, BARRIER_ARRAYCOPY, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NORMAL|ON_STRONG_OOP_REF|IN_HEAP|IS_ARRAY|ARRAYCOPY_CHECKCAST|ARRAYCOPY_DISJOINT);\
 instantiate(name, BARRIER_ARRAYCOPY, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NORMAL|ON_STRONG_OOP_REF|IN_HEAP|IS_ARRAY|ARRAYCOPY_ARRAYOF);\
 instantiate(name, BARRIER_ARRAYCOPY, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|MO_UNORDERED|AS_NORMAL|ON_STRONG_OOP_REF|IN_HEAP|IS_ARRAY|IS_DEST_UNINITIALIZED|ARRAYCOPY_ARRAYOF);\
 instantiate(name, BARRIER_ARRAYCOPY, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|MO_UNORDERED|AS_NORMAL|ON_STRONG_OOP_REF|IN_HEAP|IS_ARRAY);\
 \
 instantiate(name, BARRIER_STORE_AT, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|MO_UNORDERED|AS_NORMAL|ON_STRONG_OOP_REF|IN_HEAP|IS_ARRAY);\
 instantiate(name, BARRIER_STORE_AT, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NORMAL|ON_STRONG_OOP_REF|IN_HEAP|IS_ARRAY);\
 instantiate(name, BARRIER_STORE_AT, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NORMAL|ON_STRONG_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_STORE_AT, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|MO_RELEASE|AS_NORMAL|ON_STRONG_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_STORE_AT, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|MO_SEQ_CST|AS_NORMAL|ON_UNKNOWN_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_STORE_AT, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_UNORDERED|AS_NORMAL|ON_UNKNOWN_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_STORE_AT, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|MO_UNORDERED|AS_NORMAL|ON_UNKNOWN_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_STORE_AT, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_SEQ_CST|AS_NORMAL|ON_STRONG_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_STORE_AT, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_SEQ_CST|AS_NORMAL|ON_UNKNOWN_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_STORE_AT, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|MO_SEQ_CST|AS_NORMAL|ON_STRONG_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_STORE_AT, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|INTERNAL_RT_USE_COMPRESSED_OOPS|MO_RELEASE|AS_NORMAL|ON_STRONG_OOP_REF|IN_HEAP);\
 instantiate(name, BARRIER_STORE_AT, INTERNAL_CONVERT_COMPRESSED_OOP|INTERNAL_VALUE_IS_OOP|MO_UNORDERED|AS_NORMAL|ON_STRONG_OOP_REF|IN_HEAP);\
 \
 instantiate(name, BARRIER_LOAD_AT, 402438ull);\
 instantiate(name, BARRIER_LOAD_AT, 401478ull);\
 instantiate(name, BARRIER_LOAD_AT, 397414ull);\
 instantiate(name, BARRIER_LOAD_AT, 397382ull);\
 instantiate(name, BARRIER_LOAD_AT, 331846ull);\
 instantiate(name, BARRIER_LOAD_AT, 303206ull);\
 instantiate(name, BARRIER_LOAD_AT, 303174ull);\
 instantiate(name, BARRIER_LOAD_AT, 299110ull);\
 instantiate(name, BARRIER_LOAD_AT, 299078ull);\
 instantiate(name, BARRIER_LOAD_AT, 287014ull);\
 instantiate(name, BARRIER_LOAD_AT, 286982ull);\
 instantiate(name, BARRIER_LOAD_AT, 402470ull);\
 instantiate(name, BARRIER_LOAD_AT, 401510ull);\
 instantiate(name, BARRIER_LOAD_AT, 331878ull);\
 instantiate(name, BARRIER_LOAD_AT, 286822ull);\
 instantiate(name, BARRIER_LOAD_AT, 286790ull);\
 instantiate(name, BARRIER_LOAD_AT, 282726ull);\
 instantiate(name, BARRIER_LOAD_AT, 282694ull);\
 instantiate(name, BARRIER_LOAD_AT, 1335398ull);\
 instantiate(name, BARRIER_LOAD_AT, 1335366ull);\
 \
 instantiate(name, BARRIER_ATOMIC_CMPXCHG_AT, 402470ull);\
 instantiate(name, BARRIER_ATOMIC_CMPXCHG_AT, 402438ull);\
 instantiate(name, BARRIER_ATOMIC_CMPXCHG_AT, 1336358ull);\
 instantiate(name, BARRIER_ATOMIC_CMPXCHG_AT, 1336326ull);\
 \
 instantiate(name, BARRIER_CLONE, 270432ull);\
 instantiate(name, BARRIER_CLONE, 270400ull);\

  instantiate2(CardTable)
  instantiate2(Epsilon)
  instantiate2(G1)
  instantiate2(Z)

#define instantiate_ac(name, decorators) \
  template bool AccessInternal::PostRuntimeDispatch<name ## BarrierSet::AccessBarrier<decorators, name ## BarrierSet>, BARRIER_ARRAYCOPY, decorators>::oop_access_barrier<HeapWordImpl*>(arrayOop, unsigned long, HeapWordImpl**, arrayOop, unsigned long, HeapWordImpl**, unsigned long);

#define instantiate_ac2(name) \
    instantiate_ac(name, 1335398ull);\
    instantiate_ac(name, 18112582ull);\
    instantiate_ac(name, 18112614ull);\
    instantiate_ac(name, 26501190ull);\
    instantiate_ac(name, 26501222ull);\
    instantiate_ac(name, 3432518ull);\
    instantiate_ac(name, 3432550ull);\
    instantiate_ac(name, 34889798ull);\
    instantiate_ac(name, 34889830ull);\
    instantiate_ac(name, 36986950ull);\
    instantiate_ac(name, 36986982ull);\
    instantiate_ac(name, 1335366ull);\

  instantiate_ac2(CardTable)
  instantiate_ac2(Epsilon)
  instantiate_ac2(G1)
  instantiate_ac2(Z)

  template <typename ReturnType>
  class Return { typedef ReturnType type; };

  typedef void (*FFF)(void*, oop);


#define instantiate_resolve_oop(decorators, ds, barrier_type, function_t) \
	template typename EnableIf<HasDecorator<ds, INTERNAL_VALUE_IS_OOP>::value, function_t>::type AccessInternal::BarrierResolver<decorators, function_t, barrier_type>::resolve_barrier_gc<ds>();

  instantiate_resolve_oop(598084ull, 598084ull, BARRIER_STORE, void(*)(void*,oop));
  instantiate_resolve_oop(598084ull, 598116ull, BARRIER_STORE, void(*)(void*,oop));
  instantiate_resolve_oop(548932ull, 548964ull, BARRIER_STORE, void(*)(void*,oop));
  instantiate_resolve_oop(548932ull, 548932ull, BARRIER_STORE, void(*)(void*,oop));
  instantiate_resolve_oop(286822ull, 286822ull, BARRIER_STORE, void(*)(void*,oop));
  instantiate_resolve_oop(286788ull, 286820ull, BARRIER_STORE, void(*)(void*,oop));
  instantiate_resolve_oop(286788ull, 286788ull, BARRIER_STORE, void(*)(void*,oop));
  instantiate_resolve_oop(282694ull, 282726ull, BARRIER_STORE, void(*)(void*,oop));
  instantiate_resolve_oop(282694ull, 282694ull, BARRIER_STORE, void(*)(void*,oop));
  instantiate_resolve_oop(2646084ull, 2646116ull, BARRIER_STORE, void(*)(void*,oop));
  instantiate_resolve_oop(2646084ull, 2646084ull, BARRIER_STORE, void(*)(void*,oop));

  instantiate_resolve_oop(598084ull, 598084ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(598084ull, 598116ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(593988ull, 594020ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(593988ull, 593988ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(548932ull, 548964ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(548932ull, 548932ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(544870ull, 544870ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(544836ull, 544868ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(544836ull, 544836ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(331878ull, 331878ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(331846ull, 331878ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(331846ull, 331846ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(331844ull, 331876ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(331844ull, 331844ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(299110ull, 299110ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(299078ull, 299110ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(299078ull, 299078ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(299076ull, 299108ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(299076ull, 299076ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(286822ull, 286822ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(286788ull, 286820ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(286788ull, 286788ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(282726ull, 282726ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(282692ull, 282724ull, BARRIER_LOAD, oop(*)(void*));
  instantiate_resolve_oop(282692ull, 282692ull, BARRIER_LOAD, oop(*)(void*));

  instantiate_resolve_oop(549924ull, 549924ull, BARRIER_ATOMIC_XCHG, oop(*)(void*,oop));
  instantiate_resolve_oop(549892ull, 549924ull, BARRIER_ATOMIC_XCHG, oop(*)(void*,oop));
  instantiate_resolve_oop(549892ull, 549892ull, BARRIER_ATOMIC_XCHG, oop(*)(void*,oop));

  instantiate_resolve_oop(402438ull, 402470ull, BARRIER_STORE_AT, void(*)(oop,ptrdiff_t,oop));
  instantiate_resolve_oop(402438ull, 402438ull, BARRIER_STORE_AT, void(*)(oop,ptrdiff_t,oop));
  instantiate_resolve_oop(401478ull, 401510ull, BARRIER_STORE_AT, void(*)(oop,ptrdiff_t,oop));
  instantiate_resolve_oop(401478ull, 401478ull, BARRIER_STORE_AT, void(*)(oop,ptrdiff_t,oop));
  instantiate_resolve_oop(287750ull, 287782ull, BARRIER_STORE_AT, void(*)(oop,ptrdiff_t,oop));
  instantiate_resolve_oop(287750ull, 287750ull, BARRIER_STORE_AT, void(*)(oop,ptrdiff_t,oop));
  instantiate_resolve_oop(287238ull, 287270ull, BARRIER_STORE_AT, void(*)(oop,ptrdiff_t,oop));
  instantiate_resolve_oop(287238ull, 287238ull, BARRIER_STORE_AT, void(*)(oop,ptrdiff_t,oop));
  instantiate_resolve_oop(286790ull, 286822ull, BARRIER_STORE_AT, void(*)(oop,ptrdiff_t,oop));
  instantiate_resolve_oop(286790ull, 286790ull, BARRIER_STORE_AT, void(*)(oop,ptrdiff_t,oop));
  instantiate_resolve_oop(1335366ull, 1335398ull, BARRIER_STORE_AT, void(*)(oop,ptrdiff_t,oop));
  instantiate_resolve_oop(1335366ull, 1335366ull, BARRIER_STORE_AT, void(*)(oop,ptrdiff_t,oop));

  instantiate_resolve_oop(402438ull, 402470ull, BARRIER_ATOMIC_CMPXCHG_AT, oop(*)(oop,ptrdiff_t,oop,oop));
  instantiate_resolve_oop(402438ull, 402438ull, BARRIER_ATOMIC_CMPXCHG_AT, oop(*)(oop,ptrdiff_t,oop,oop));
  instantiate_resolve_oop(1336326ull, 1336358ull, BARRIER_ATOMIC_CMPXCHG_AT, oop(*)(oop,ptrdiff_t,oop,oop));
  instantiate_resolve_oop(1336326ull, 1336326ull, BARRIER_ATOMIC_CMPXCHG_AT, oop(*)(oop,ptrdiff_t,oop,oop));

  instantiate_resolve_oop(402438ull, 402470ull, BARRIER_LOAD_AT, oop(*)(oop,ptrdiff_t));
  instantiate_resolve_oop(402438ull, 402438ull, BARRIER_LOAD_AT, oop(*)(oop,ptrdiff_t));
  instantiate_resolve_oop(401478ull, 401510ull, BARRIER_LOAD_AT, oop(*)(oop,ptrdiff_t));
  instantiate_resolve_oop(401478ull, 401478ull, BARRIER_LOAD_AT, oop(*)(oop,ptrdiff_t));
  instantiate_resolve_oop(397382ull, 397414ull, BARRIER_LOAD_AT, oop(*)(oop,ptrdiff_t));
  instantiate_resolve_oop(397382ull, 397382ull, BARRIER_LOAD_AT, oop(*)(oop,ptrdiff_t));
  instantiate_resolve_oop(331846ull, 331878ull, BARRIER_LOAD_AT, oop(*)(oop,ptrdiff_t));
  instantiate_resolve_oop(331846ull, 331846ull, BARRIER_LOAD_AT, oop(*)(oop,ptrdiff_t));
  instantiate_resolve_oop(303174ull, 303206ull, BARRIER_LOAD_AT, oop(*)(oop,ptrdiff_t));
  instantiate_resolve_oop(303174ull, 303174ull, BARRIER_LOAD_AT, oop(*)(oop,ptrdiff_t));
  instantiate_resolve_oop(299078ull, 299110ull, BARRIER_LOAD_AT, oop(*)(oop,ptrdiff_t));
  instantiate_resolve_oop(299078ull, 299078ull, BARRIER_LOAD_AT, oop(*)(oop,ptrdiff_t));
  instantiate_resolve_oop(286982ull, 287014ull, BARRIER_LOAD_AT, oop(*)(oop,ptrdiff_t));
  instantiate_resolve_oop(286982ull, 286982ull, BARRIER_LOAD_AT, oop(*)(oop,ptrdiff_t));
  instantiate_resolve_oop(286790ull, 286822ull, BARRIER_LOAD_AT, oop(*)(oop,ptrdiff_t));
  instantiate_resolve_oop(286790ull, 286790ull, BARRIER_LOAD_AT, oop(*)(oop,ptrdiff_t));
  instantiate_resolve_oop(282694ull, 282726ull, BARRIER_LOAD_AT, oop(*)(oop,ptrdiff_t));
  instantiate_resolve_oop(282694ull, 282694ull, BARRIER_LOAD_AT, oop(*)(oop,ptrdiff_t));
  instantiate_resolve_oop(1335366ull, 1335398ull, BARRIER_LOAD_AT, oop(*)(oop,ptrdiff_t));
  instantiate_resolve_oop(1335366ull, 1335366ull, BARRIER_LOAD_AT, oop(*)(oop,ptrdiff_t));

  instantiate_resolve_oop(36986950ull, 36986982ull, BARRIER_ARRAYCOPY, bool(*)(arrayOop,size_t,HeapWordImpl**,arrayOop,size_t,HeapWordImpl**,size_t));
  instantiate_resolve_oop(36986950ull, 36986950ull, BARRIER_ARRAYCOPY, bool(*)(arrayOop,size_t,HeapWordImpl**,arrayOop,size_t,HeapWordImpl**,size_t));
  instantiate_resolve_oop(34889798ull, 34889830ull, BARRIER_ARRAYCOPY, bool(*)(arrayOop,size_t,HeapWordImpl**,arrayOop,size_t,HeapWordImpl**,size_t));
  instantiate_resolve_oop(34889798ull, 34889798ull, BARRIER_ARRAYCOPY, bool(*)(arrayOop,size_t,HeapWordImpl**,arrayOop,size_t,HeapWordImpl**,size_t));
  instantiate_resolve_oop(3432518ull, 3432550ull, BARRIER_ARRAYCOPY, bool(*)(arrayOop,size_t,HeapWordImpl**,arrayOop,size_t,HeapWordImpl**,size_t));
  instantiate_resolve_oop(3432518ull, 3432518ull, BARRIER_ARRAYCOPY, bool(*)(arrayOop,size_t,HeapWordImpl**,arrayOop,size_t,HeapWordImpl**,size_t));
  instantiate_resolve_oop(26501190ull, 26501222ull, BARRIER_ARRAYCOPY, bool(*)(arrayOop,size_t,HeapWordImpl**,arrayOop,size_t,HeapWordImpl**,size_t));
  instantiate_resolve_oop(26501190ull, 26501190ull, BARRIER_ARRAYCOPY, bool(*)(arrayOop,size_t,HeapWordImpl**,arrayOop,size_t,HeapWordImpl**,size_t));
  instantiate_resolve_oop(18112582ull, 18112614ull, BARRIER_ARRAYCOPY, bool(*)(arrayOop,size_t,HeapWordImpl**,arrayOop,size_t,HeapWordImpl**,size_t));
  instantiate_resolve_oop(18112582ull, 18112582ull, BARRIER_ARRAYCOPY, bool(*)(arrayOop,size_t,HeapWordImpl**,arrayOop,size_t,HeapWordImpl**,size_t));
  instantiate_resolve_oop(1335366ull, 1335398ull, BARRIER_ARRAYCOPY, bool(*)(arrayOop,size_t,HeapWordImpl**,arrayOop,size_t,HeapWordImpl**,size_t));
  instantiate_resolve_oop(1335366ull, 1335366ull, BARRIER_ARRAYCOPY, bool(*)(arrayOop,size_t,HeapWordImpl**,arrayOop,size_t,HeapWordImpl**,size_t));

  instantiate_resolve_oop(283654ull, 283686ull, BARRIER_ATOMIC_CMPXCHG, oop(*)(void*,oop,oop));
  instantiate_resolve_oop(283654ull, 283654ull, BARRIER_ATOMIC_CMPXCHG, oop(*)(void*,oop,oop));

#define instantiate_resolve(decorators, ds, barrier_type, function_t) \
  template typename EnableIf<!HasDecorator<ds, INTERNAL_VALUE_IS_OOP>::value, function_t>::type AccessInternal::BarrierResolver<decorators, function_t, barrier_type>::resolve_barrier_gc<ds>();

  instantiate_resolve(270400ull, 270432ull, BARRIER_CLONE, void(*)(oop,oop,size_t));
  instantiate_resolve(270400ull, 270400ull, BARRIER_CLONE, void(*)(oop,oop,size_t));

};
