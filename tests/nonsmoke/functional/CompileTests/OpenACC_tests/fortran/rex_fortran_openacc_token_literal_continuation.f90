subroutine rex_fortran_openacc_token_literal_continuation()
  implicit none
  integer :: value01, value02, value03, value04, value05, value06
  integer :: value07, value08, value09, value10, value11, value12
  integer :: value13, value14, value15, value16, value17, value18

!$acc parallel copy(value01, value02, value03, value04, value05, val&
!$acc&ue06, value07, value08, value09, value10, value11, value12, &
!$acc& value13, value14, value15, value16, value17, value18) async(1&
!$acc&2) if(.true.)
  value01 = value18
!$acc end parallel
end subroutine rex_fortran_openacc_token_literal_continuation
