#define REX_LEXICAL_LIMIT 4
#define REX_LEXICAL_EXTENSION $ompx rex_fortran_lexical_macro_opaque
#define REX_LEXICAL_OMPX_PREFIX $ompx
#define REX_LEXICAL_PARALLEL parallel
program rex_fortran_lexical_free
  implicit none
  include 'rex_fortran_lexical_parameters.inc'
  integer :: i, temporary, total
  character(len=*), parameter :: message = &
    'free literal ! with ''quotes'' and a con&
&tinued character token'
  total = 0
! Trailing preprocessing tokens must not become executable source.
#include "rex_fortran_lexical_empty.inc" ; total = -999
! rex_fortran_lexical_inactive_begin_free_source
#if 0
this inactive source is intentionally not valid Fortran
!$omp taskwait
!$ompx rex_fortran_lexical_inactive_free_source_opaque
#endif
! rex_fortran_lexical_inactive_end_free_source
!$ompx rex_fortran_lexical_opaque &
!$ompx& continued_payload
!REX_LEXICAL_EXTENSION
!REX_LEXICAL_OMPX_PREFIX rex_fortran_lexical_prefix_opaque & ! rex_fortran_lexical_directive_inline_ompx
! rex_fortran_lexical_directive_between_ompx

!$ompx& prefix_continued_payload
!$omp REX_LEXICAL_PARALLEL do reduction(+:total) & ! rex_fortran_lexical_directive_inline_free
! rex_fortran_lexical_directive_between_free
! rex_fortran_lexical_inactive_begin_free_continuation
#if 0
this inactive continuation gap is intentionally not valid Fortran
!$omp barrier
!$ompx rex_fortran_lexical_inactive_free_continuation_opaque
#endif
! rex_fortran_lexical_inactive_end_free_continuation

!$omp& private(temporary) REX_LEXICAL_EMPTY("!") ! rex_fortran_lexical_directive_empty_free_comment
  do i = 1, REX_LEXICAL_LIMIT
    temporary = i
    total = total + temporary
  end do
!$omp end parallel do
!$ total = total + &
!$& 1 ! rex_fortran_lexical_real_comment_free
!$ total = total + 8
  call rex_fortran_lexical_add( &
!$ 10, &
    rex_lexical_increment, total)
#ifdef _OPENMP
  if (total /= 49) stop 1
#else
  if (total /= 30) stop 2
#endif
  print '(I0,1X,A)', total, message
end program rex_fortran_lexical_free

subroutine rex_fortran_lexical_add( &
!$ first, &
  second, total)
  implicit none
!$ integer, intent(in) :: first
  integer, intent(in) :: second
  integer, intent(inout) :: total
!$ total = total + first
  total = total + second
end subroutine rex_fortran_lexical_add
