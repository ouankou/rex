#define REX_NATIVE_INCLUDE include
program rex_fortran_include_macro_native
  implicit none
  REX_NATIVE_INCLUDE 'rex_fortran_include_macro_native.inc'
  print *, rex_include_value
end program
