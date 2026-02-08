program test_protected
  use protected_mod
  implicit none
  integer :: local
  local = pval
  qval = 1
  call set_pval(3)
end program test_protected
