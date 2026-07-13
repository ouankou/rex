program rex_fortran_intrinsic_use_derived_type_publication
  use, intrinsic :: iso_c_binding, only : c_null_ptr, c_ptr, &
       c_null_funptr, c_funptr
  implicit none

  type(c_ptr) :: data_address
  type(c_funptr) :: procedure_address

  data_address = c_null_ptr
  procedure_address = c_null_funptr
end program rex_fortran_intrinsic_use_derived_type_publication
