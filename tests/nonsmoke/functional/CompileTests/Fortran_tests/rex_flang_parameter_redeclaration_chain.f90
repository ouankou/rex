program rex_flang_parameter_redeclaration_chain
  use iso_c_binding, only: c_f_pointer, c_ptr
  implicit none

  type(c_ptr) :: address
  integer, pointer :: value

  call c_f_pointer(address, value)
end program rex_flang_parameter_redeclaration_chain
