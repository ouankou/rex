
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

extern "C++" {
int __checkType(...);
int __checkCalleeDefnLine(...);
}

// ELSA's historical xfailure marker should not occupy the ordinary identifier
// namespace; some parser specimens intentionally reuse the same spelling as a
// typedef to exercise declaration/expression disambiguation.
#define __rose_elsa_xfailure_0() 0
#define __rose_elsa_xfailure_1(arg) int arg
#define __rose_elsa_xfailure_select(_0, _1, name, ...) name
#define __cause_xfailure(...)                                                  \
  __rose_elsa_xfailure_select(_, ##__VA_ARGS__, __rose_elsa_xfailure_1,        \
                              __rose_elsa_xfailure_0)(__VA_ARGS__)

#define __testOverload(expr, expected) ((int)sizeof(((void)(expr)), 0))

#endif
