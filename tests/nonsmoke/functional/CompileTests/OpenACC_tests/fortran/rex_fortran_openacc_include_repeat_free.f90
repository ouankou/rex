  program rex_fortran_openacc_include_repeat_free
  implicit none
  integer value
  value = 7
! REX_INCLUDE_BEFORE
!$acc parallel ! REX_INCLUDE_OUTER_BEGIN
! REX_INCLUDE_FIRST
  include 'rex_fortran_openacc_include_turnover.inc'
! REX_INCLUDE_MIDDLE
  include 'rex_fortran_openacc_include_turnover.inc'
! REX_INCLUDE_LAST
!$acc end parallel ! REX_INCLUDE_OUTER_END
! REX_INCLUDE_AFTER
  if (value .ne. 7) stop 1
  print *, value
  end
