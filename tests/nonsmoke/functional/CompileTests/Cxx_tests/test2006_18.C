/*
This is a problem arising from the fact that ROSE (legacy frontend) and gcc
triggers the generation of different files. In the following example the
preprocessing token 'isdef' is defined in the GCC preprocessor and NOT
DEFINED in the legacy frontend preprocessor. This is no problem when ROSE or GCC
parses it, but when the ROSE GCC backend tries to compile the output from
ROSE it gives the error specified below because ROSE has expanded an
macro.

To say the least, this was a tricky bug to locate.

Compiling the following code using ROSE:
*/

// Skip version 4.x gnu compilers
// #if ( __GNUC__ == 3 )
#if (defined(__clang__) == 0 && __GNUC__ == 3)

#include <fstream>

#include <math.h>

// #if ( (__GNUC__ == 3) && (__GNUC_MINOR__ < 4) )
#if (defined(__clang__) == 0 && __GNUC__ == 3 && __GNUC_MINOR__ < 4)

#ifdef isfinite
#warning isdef isfinite
#define is_finite(x) (isfinite(x))
#else /* !defined(isfinite) */
#warning isndef isfinite
#define is_finite(x) (long_double_is_finite(x)) /* See definition below. */
#define NEED_LONG_DOUBLE_IS_FINITE 1
#endif /* ifdef isfinite */

#ifdef NEED_LONG_DOUBLE_IS_FINITE

static bool long_double_is_finite(long double value) {
  return 1;
} /* long_double_is_finite */

#endif /* ifdef NEED_LONG_DOUBLE_IS_FINITE */

static void conv_host_fp_to_float() {
  if (is_finite(1.0)) {
  }
} /* conv_host_fp_to_float */

#else
#warning "Case not tested for version 3.4 and higher."
#endif

#else
#warning "Not tested on gnu 4.0 or greater versions"
#endif
