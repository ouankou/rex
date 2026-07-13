subroutine rex_fortran_openacc_free_directive_continuation()
  implicit none
  integer :: value01, value02, value03, value04, value05, value06
  integer :: value07, value08, value09, value10, value11, value12
  integer :: value13, value14, value15, value16, value17, value18

!$acc parallel copy(value01, value02, value03, value04, value05, value06, &
!$acc& value07, value08, value09, value10, value11, value12, value13, &
!$acc& value14, value15, value16, value17, value18)
  value01 = value18
!$acc end parallel
end subroutine rex_fortran_openacc_free_directive_continuation
