program rex_fortran_associate_end_label
  implicit none
  integer :: value

  value = 1
  associate (alias => value)
    alias = 2
    if (alias == 2) goto 10
10 end associate
  if (value /= 2) error stop
end program rex_fortran_associate_end_label
