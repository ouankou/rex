program rex_unparser_fortran_pointer_comment
  implicit none
  integer, target :: target = 1
  !! REX pointer initialization
  integer, pointer :: pointer => target

  if (pointer /= 1) error stop
end program rex_unparser_fortran_pointer_comment
