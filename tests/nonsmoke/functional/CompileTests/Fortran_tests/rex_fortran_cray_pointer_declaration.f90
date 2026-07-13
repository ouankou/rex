program rex_fortran_cray_pointer_declaration
  implicit none
  integer(kind=8) :: address
  integer :: pointee
  pointer(address, pointee)
  address = 0
end program rex_fortran_cray_pointer_declaration
