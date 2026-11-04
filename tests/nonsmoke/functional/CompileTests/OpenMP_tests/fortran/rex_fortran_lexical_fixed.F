#define REX_LEXICAL_LIMIT 4
      program rex_fortran_lexical_fixed
      implicit none
      include 'rex_fortran_lexical_parameters.inc'
      integer i, temporary, total
      total = 0
c     Trailing preprocessing tokens must remain ignored.
#include "rex_fortran_lexical_empty.inc" ; total = -999
! rex_fortran_lexical_inactive_begin_fixed
#if 0
this inactive source is intentionally not valid Fortran
c$omp barrier
*$ompx rex_fortran_lexical_inactive_fixed_opaque
#endif
! rex_fortran_lexical_inactive_end_fixed
c$omp parallel do reduction(+:total)
*$omp& private(temporary)
      do i = 1, REX_LEXICAL_LIMIT
        temporary = i
        total = total + temporary
      end do
!$omp end parallel do
c$    total = total + 1 ! rex_fortran_lexical_real_comment_fixed
*$    total = total + 2
!$    go to 123
!$    total = -999
!$123 total = total + 4
      call rex_fortran_lexical_add(
c$   & 10,
     & rex_lexical_increment, total)
#ifdef _OPENMP
      if (total .ne. 47) stop 1
#else
      if (total .ne. 30) stop 2
#endif
      print '(I0,1X,A)', total, 'fixed literal ! with ''quotes'''
      end

      subroutine rex_fortran_lexical_add(
*$   & first,
     & second, total)
      implicit none
c$    integer, intent(in) :: first
      integer, intent(in) :: second
      integer, intent(inout) :: total
!$    total = total + first
      total = total + second
      end
