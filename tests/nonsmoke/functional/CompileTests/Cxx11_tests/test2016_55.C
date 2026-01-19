double XXX__builtin_fabs(double) __attribute__((nothrow, const));
float XXX__builtin_acosf(float) __attribute__((nothrow, const));

double __builtin_fabs(double) __attribute__((nothrow, const));

inline constexpr double
my_abs(double __x)
   {
     return __builtin_fabs(__x);
   }

