subroutine rex_fortran_openmp_free_directive_continuation()
  implicit none
  integer :: value01, value02, value03, value04, value05, value06
  integer :: value07, value08, value09, value10, value11, value12
  integer :: value13, value14, value15, value16, value17, value18

!$omp target map(tofrom: value01, value02, value03, value04, value05, &
!$omp& value06, value07, value08, value09, value10, value11, value12, &
!$omp& value13, value14, value15, value16, value17, value18)
  value01 = value18
!$omp end target
end subroutine rex_fortran_openmp_free_directive_continuation
