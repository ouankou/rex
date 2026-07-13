program rex_fortran_zero_iteration_implied_do
  implicit none
  integer :: counter
  integer :: local_count(1)
  integer :: local_map(1)

  local_count = 1
  local_map = (/ 1, (local_count(counter), counter = 1, 0) /)
  if (local_map(1) /= 1) error stop
end program rex_fortran_zero_iteration_implied_do
