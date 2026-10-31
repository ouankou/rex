! EXT-FLANG-INACTIVE-DATAREF: array intrinsic operands exercise the exact
! CheckExplicitInterface caller. See https://github.com/ouankou/rex/issues/885.
subroutine rex_probe(a, b)
  real :: a(2), b(2)
  a = min(abs(b), abs(b))
end subroutine
