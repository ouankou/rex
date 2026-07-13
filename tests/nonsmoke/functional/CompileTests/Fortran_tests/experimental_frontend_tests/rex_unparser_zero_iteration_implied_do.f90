program rex_unparser_zero_iteration_implied_do
  implicit none
  integer :: i
  integer, allocatable :: values(:)

  values = [ (i, i = 4, 0, 11) ]
  if (size(values) /= 0) error stop
end program rex_unparser_zero_iteration_implied_do
