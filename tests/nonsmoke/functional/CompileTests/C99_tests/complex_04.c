
/*
   This test code fails for a mysterious reason, if any line is removed
   a file_info object is not generated correctly and the translation fails :-).
   This may be an legacy frontend problem (this codes was simplified from
   complex_01.c).

   This is a C and C99 test code of the use of complex types.
   It works because the rose_required_macros_and_functions.h
   file redefines __complex__ using:
   #define __complex__ _Complex
 */

/* Relavant C99 types:
   "_Bool", "_Complex", "__I__", "__NAN__", "__INFINITY__",
*/

#include <complex.h>
int main(void) {
  // Older GNU systax for declaration of complex variables (specification of
  // complex types)
  __complex__ long double z_old_syntax = 3.0;

  // __complex__ float an_i_old_syntax = __I__;
  _Complex float an_i_new_syntax = __I__;

  // __I__;

  // ROSE does not yet support the imaginary add operator
  _Complex float a_complex_value = 0.0;

  // Newer syntax for specification of complex types
  _Complex float x = 1.0;
  _Complex double y = 2.0;
  _Complex long double z = 3.0;

  // Specification of complex literals is a bit more complicated
  // (not clear if this is might just be the use of the commar operator).
  // note that the parenthesis are required.
  _Complex float x_with_real_and_imaginary_parts = (1.0, -1.0);

  _Complex float an_i_new_syntax2 = __I__;

  __I__;

  float x_imaginary = __imag__ x;
  double y_imaginary = __imag__ y;
  long double z_imaginary = __imag__ z;
  (void)x_imaginary;
  (void)y_imaginary;
  (void)z_imaginary;

  _Complex float x1 = 1.0;
  _Complex double y1 = 2.0;

  // This fails, but adding the body to an explicitly defined block works just
  // fine?????
  if (x1 != y1) // complex inequality operator
    x1 = x1;

  return 0;
}
