program rex_flang_compiler_inline_comment
  implicit none
  integer :: index, total
  total = 0
!dir$ unroll ! REX_COMPILER_INLINE_COMMENT
  do index = 1, 4
    total = total + index
  end do
#define REX_EMPTY(x)
!dir$ unroll REX_EMPTY("!") ! REX_COMPILER_EMPTY_INLINE_COMMENT
  do index = 1, 1
    total = total + 0
  end do
!dir$ un&
! REX_COMPILER_GAP_COMMENT

!dir$ &roll ! REX_COMPILER_CONTINUED_INLINE_COMMENT
  do index = 1, 1
    total = total + 0
  end do
#define REX_REST !dir$ &roll
!dir$ un&
REX_REST ! REX_COMPILER_MACRO_INLINE_COMMENT
  do index = 1, 1
    total = total + 0
  end do
  print '(I0)', total
end program
