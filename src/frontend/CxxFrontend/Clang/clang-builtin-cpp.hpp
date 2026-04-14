
#ifndef SKIP_ROSE_BUILTIN_DECLARATIONS

// Do not inject `restrict` into C++: it is not a C++ keyword, and forcing it
// into every translation unit breaks valid user identifiers.

#ifndef aligned
#define aligned(x) __aligned__(((x) == 0) ? 1 : (x))
#endif

namespace std {}

#endif
