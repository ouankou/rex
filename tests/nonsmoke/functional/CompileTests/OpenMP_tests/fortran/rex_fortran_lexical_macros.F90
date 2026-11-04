#define REX_LEXICAL_SPLIT 7
#define REX_LEXICAL_EPOCH 3
#define REX_LEXICAL_OMP_PREFIX $omp
#include "rex_fortran_lexical_empty.inc"
program rex_fortran_lexical_macros
  implicit none
  integer :: total
! rex_fortran_lexical_inactive_begin_macros
#if 0
this inactive source is intentionally not valid Fortran
!$omp barrier
!$ompx rex_fortran_lexical_inactive_macros_opaque
#endif
! rex_fortran_lexical_inactive_end_macros
  total = REX_LEXICAL_SPL&
&IT ! rex_fortran_lexical_directive_macro_terminal_comment
  total = total + REX_LEXICAL_EPOCH + &
#undef REX_LEXICAL_EPOCH
#define REX_LEXICAL_EPOCH 5
    REX_LEXICAL_EPOCH
!REX_LEXICAL_OMP_PREFIX parallel num_thr& ! rex_fortran_lexical_directive_inline_macros
! rex_fortran_lexical_directive_between_macros

!$omp&eads(REX_LEXICAL_ALIAS(1)) shared(total) REX_LEXICAL_EMPTY("!") ! rex_fortran_lexical_directive_empty_macro_comment
  total = total + REX_LEXICAL_ALIAS(1)
!REX_LEXICAL_OMP_PREFIX end parallel
!$ total = total + 10
  REX_LEXICAL_WARNING_IF(total > 0, "rex warning\npayload")
  REX_LEXICAL_WARNING_IF(total > 0, "rex warning\npayload")
  REX_LEXICAL_IGNORED_WARNING_IF(total < 0, "rex ignored\npayload");
  REX_LEXICAL_CAPTURE(total + (1))
  REX_LEXICAL_CAPTURE()
  REX_LEXICAL_VARIADIC(1, 2, 3)
  REX_LEXICAL_COMMENT
  print '(I0)', total
end program rex_fortran_lexical_macros

subroutine rex_lexical_record(message, filename, line)
  implicit none
  character(len=*), intent(in) :: message, filename
  integer, intent(in) :: line
  if (len_trim(filename) == 0 .or. line <= 0) stop 5
  print '(A)', message
end subroutine rex_lexical_record

subroutine rex_lexical_text(text)
  implicit none
  character(len=*), intent(in) :: text
  print '(I0,1X,A)', len(text), text
end subroutine rex_lexical_text

subroutine rex_lexical_values(first, second, third)
  implicit none
  integer, intent(in) :: first, second, third
  if (first /= 1 .or. second /= 2 .or. third /= 3) stop 6
  print '(3(I0,1X))', first, second, third
end subroutine rex_lexical_values
