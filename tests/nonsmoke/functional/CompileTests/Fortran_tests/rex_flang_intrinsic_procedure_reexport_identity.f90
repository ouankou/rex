program rex_flang_intrinsic_procedure_reexport_identity
  use, intrinsic :: iso_c_binding, only : c_associated, c_f_pointer, &
      c_loc, c_ptr
  implicit none

  integer, target :: scalar
  integer, pointer :: view
  type(c_ptr) :: address

  scalar = 7
  address = c_loc(scalar)
  if (.not. c_associated(address)) stop 1
  call c_f_pointer(address, view)
end program rex_flang_intrinsic_procedure_reexport_identity
