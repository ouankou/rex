program rex_fortran_block_end_label
  implicit none
  integer :: value

  value = 1
  block
    if (value == 1) goto 40
    value = 0
40 end block
  if (value /= 1) error stop
end program rex_fortran_block_end_label
