
#include <fstream>

#include <math.h>

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
