      program rex_fortran_openacc_include_empty_fixed
      implicit none
      integer value
      value = 7
! REX_INCLUDE_BEFORE
      include 'rex_fortran_openacc_include_begin.inc'
! REX_INCLUDE_BODY_MAIN
      include 'rex_fortran_openacc_include_comments.inc' ! REX_INCLUDE_T
! REX_INCLUDE_END_BEFORE
!$acc end parallel ! REX_INCLUDE_END_INLINE
! REX_INCLUDE_AFTER
      if (value .ne. 7) stop 1
      print *, value
      end
