program rex_fortran_iso_c_binding_intrinsic_consumer
  use, intrinsic :: iso_c_binding, only: c_int
  implicit none
  integer(c_int) :: value

  value = 1_c_int
end program rex_fortran_iso_c_binding_intrinsic_consumer
