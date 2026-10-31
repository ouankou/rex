! EXT-FLANG-INACTIVE-DATAREF: https://github.com/ouankou/rex/issues/885
! Independent flang -fc1 reproduction of inactive optional storage.
program rex_flang_inactive_intrinsic
  integer, parameter :: real_kind = kind(1.0)
  real(kind=real_kind) :: value = 1.0
  if (value /= 1.0) stop 1
end program
