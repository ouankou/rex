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
  print '(I0)', total
end program rex_fortran_lexical_macros
