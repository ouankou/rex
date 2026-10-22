program rex_fortran_where_elsewhere_chain
  implicit none
  integer :: values(4)
  logical :: first(4), second(4), third(4)

  first = [.true., .false., .false., .false.]
  second = [.false., .true., .false., .false.]
  third = [.false., .false., .true., .false.]
  values = 0

10 chain: where (first)
    values = 1
20 elsewhere (second)
    values = 2
30 elsewhere (third)
    values = 3
40 elsewhere
    values = 4
50 end where chain

  if (any(values /= [1, 2, 3, 4])) error stop
end program rex_fortran_where_elsewhere_chain
