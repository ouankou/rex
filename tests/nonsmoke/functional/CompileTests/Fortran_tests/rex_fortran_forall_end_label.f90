program rex_fortran_forall_end_label
  implicit none
  integer :: i, values(3)

  values = 0
  forall (i = 1:3)
    values(i) = i
20 end forall

  if (any(values /= [1, 2, 3])) error stop
end program rex_fortran_forall_end_label
