#ifdef assert
#undef assert
#endif

#define assert(p, ...) vmassert(p, __VA_ARGS__)
