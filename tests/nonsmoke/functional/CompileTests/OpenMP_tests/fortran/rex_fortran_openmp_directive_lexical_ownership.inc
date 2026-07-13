module rex_directive_owner_mod
  implicit none
  integer :: rex_before = 1
!$omp declare target to(rex_before)
  integer :: rex_after = 2
end module rex_directive_owner_mod
