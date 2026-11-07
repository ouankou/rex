program rex_fortran_include_cpp_state
  implicit none
!$omp barrier ! REX_SELECTED_DISABLED
#include "rex_fortran_include_cpp_state.inc"
#if defined(REX_INCLUDE_VALUE) && REX_INCLUDE_VALUE == 7
  print *, 7
#else
  print *, 99
#endif
end program
