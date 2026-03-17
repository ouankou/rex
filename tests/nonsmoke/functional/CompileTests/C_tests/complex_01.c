/* This is a C and C99 test code of the use of complex types.
   It works because the rose_required_macros_and_functions.h
   file redefines __complex__ using:
   #define __complex__ _Complex
 */

/* Relavant C99 types:
   "_Bool", "_Complex", "_Complex_I", "NAN", "INFINITY",
*/

#include <complex.h>
int main(void) {
  // Older GNU systax for declaration of complex variables (specification of
  // complex types)
  __complex__ float x_old_syntax = 1.0;
  __complex__ double y_old_syntax = 2.0;
  __complex__ long double z_old_syntax = 3.0;

  // __complex__ float an_i_old_syntax = _Complex_I;
  _Complex float an_i_new_syntax = _Complex_I;

  _Complex_I;

  // ROSE does not yet support the imaginary add operator
  _Complex float a_complex_value = 0.0;

  // a_complex_value = 4.0;

  // a_complex_value = 3.0f + (4.0f * _Complex_I);
  // a_complex_value = 3.0f - 4.0f * _Complex_I;
  // a_complex_value = 3.0f * (4.0f * _Complex_I);
  // a_complex_value = 3.0f / (4.0f * _Complex_I);

  // Newer syntax for specification of complex types
  _Complex float x = 1.0;
  _Complex double y = 2.0;
  _Complex long double z = 3.0;

  // Spell a complex literal using standard complex arithmetic.
  _Complex float x_with_real_and_imaginary_parts = 1.0f - 1.0f * _Complex_I;

  return 0;
}
