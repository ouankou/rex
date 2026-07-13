      subroutine rex_fortran_openacc_fixed_directive_continuation()
      implicit none
      integer value01, value02, value03, value04, value05, value06
      integer value07, value08, value09, value10, value11, value12
      integer value13, value14, value15, value16, value17, value18

!$acc parallel copy(value01, value02, value03, value04, value05,
!$acc& value06, value07, value08, value09, value10, value11,
!$acc& value12, value13, value14, value15, value16, value17,
!$acc& value18)
      value01 = value18
!$acc end parallel
      end
