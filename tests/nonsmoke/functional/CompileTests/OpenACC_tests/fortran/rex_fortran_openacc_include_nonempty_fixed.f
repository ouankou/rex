      program rex_fortran_openacc_include_nonempty_fixed
      implicit none
      integer value
      value = 7
! REX_INCLUDE_BEFORE
      include 'rex_fortran_openacc_include_begin.inc'
! REX_INCLUDE_BODY_MAIN
      continue
! REX_INCLUDE_END_BEFORE
!$acc end parallel ! REX_INCLUDE_END_INLINE
! REX_INCLUDE_AFTER
      if (value .ne. 7) stop 1
      print *, value
      end
