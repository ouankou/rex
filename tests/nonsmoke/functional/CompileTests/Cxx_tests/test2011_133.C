// These functions were introduced in GCC 3.3
// You need to add -mmmx and -msse to the compiler
// configuration (for gcc to enable these).

// int __builtin_ia32_paddq(long long,long long);

#ifdef __INTEL_COMPILER
// Added type for Intel compilers.
// typedef __m64 v2si;
// typedef unsigned long int __m64;
// typedef __v2si __m64;
// #define __m64
#endif

#ifdef __MMX__
// #error "Note: __MMX__ is defined"

#include <emmintrin.h>
#endif

int main() { return 0; }
