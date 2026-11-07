#define REX_SCOPE_ROOT_VALUE 9
#define REX_SCOPE_THREADS 4
program rex_fortran_include_selected_scope
  implicit none
  integer :: total
! REX_SELECTED_CONTEXT_COMMENT
  include 'rex_fortran_include_selected_native.inc'
#include "rex_fortran_include_selected_cpp.inc"
  total = 0
  include 'rex_fortran_include_selected_execution.inc'
!$omp end parallel
  print '(3(I0,1X))', native_value, cpp_value, total
end program
! REX_SELECTED_CONTEXT_EOF
