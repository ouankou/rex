
#ifndef SKIP_ROSE_BUILTIN_DECLARATIONS

#ifndef restrict
#define restrict __restrict__
#endif

#ifndef aligned
#define aligned(x) __aligned__(((x) == 0) ? 1 : (x))
#endif

namespace std {}

#endif
