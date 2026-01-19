// DQ (9/10/2016): Checking how _GLIBCXX_CONSTEXPR is being set for C++11 support.
#include <bits/c++config.h>
// #include "rose_c++config.h"

#ifdef _GLIBCXX_CONSTEXPR
   #warning "_GLIBCXX_CONSTEXPR IS defined"
#else
   #warning "_GLIBCXX_CONSTEXPR is NOT defined"
#endif

#warning "_GLIBCXX_CONSTEXPR " _GLIBCXX_CONSTEXPR
