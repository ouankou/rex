program rex_test2026_equivalence
  implicit none
  integer :: i, j
  integer :: arr(2)
  equivalence (i, arr(1)), (j, arr(2))
  print *, i, j, arr(1), arr(2)
end program rex_test2026_equivalence
