program rex_test2026_coarray_image_selector
  use iso_fortran_env, only: team_type
  implicit none
  integer :: a[*]
  integer :: stat
  type(team_type) :: team
  integer :: team_number
  a[1, stat=stat] = 1
  a[1, stat=stat, team=team] = 2
  a[1, stat=stat, team_number=team_number] = 3
end program rex_test2026_coarray_image_selector
