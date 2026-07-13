
#ifndef SKIP_ROSE_BUILTIN_DECLARATIONS

// Do not inject `restrict` into C++: it is not a C++ keyword, and forcing it
// into every translation unit breaks valid user identifiers.

#ifndef aligned
#define aligned(x) __aligned__(((x) == 0) ? 1 : (x))
#endif

#if !defined(__FLOAT128__) && !defined(__SIZEOF_FLOAT128__) &&                 \
    defined(__LDBL_MANT_DIG__) && __LDBL_MANT_DIG__ == 113
#ifndef __float128
#define __float128 long double
#endif
#endif

namespace std {}

#endif
