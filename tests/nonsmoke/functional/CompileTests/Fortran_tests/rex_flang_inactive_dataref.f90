! EXT-FLANG-INACTIVE-DATAREF: https://github.com/ouankou/rex/issues/885
! Also reproduces independently with flang -fc1 -fsyntax-only under Memcheck.
subroutine rex_evaluate(lhs, rhs, result)
  real, intent(in) :: lhs, rhs
  real, intent(out) :: result
  result = lhs + 2 * rhs
end subroutine

program rex_flang_inactive_dataref
  real :: result
  call rex_evaluate(1.0, 2.0, result)
  if (result /= 5.0) stop 1
end program
