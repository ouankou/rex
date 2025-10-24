
// FIXME Clang know __builtin_va_list but not __builtin_va_start and __builtin_va_end, why?

// Note that this permits the generated graps to be as small as possible where
// the input code is defined using -DSKIP_ROSE_BUILTIN_DECLARATIONS
#ifndef SKIP_ROSE_BUILTIN_DECLARATIONS

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#if !__has_builtin(__builtin_va_start)
void __builtin_va_start(__builtin_va_list, ...);
#endif

#if !__has_builtin(__builtin_va_end)
void __builtin_va_end(__builtin_va_list);
#endif

#if !__has_builtin(__builtin_alloca)
void *__builtin_alloca(__SIZE_TYPE__ size);
#endif

#endif
