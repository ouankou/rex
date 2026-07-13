module rex_flang_intrinsic_module_type_provider
  implicit none
contains
  subroutine rex_accept_intrinsic_pointer(rex_intrinsic_pointer)
    use, intrinsic :: iso_c_binding, only : c_ptr
    type(c_ptr), value :: rex_intrinsic_pointer
  end subroutine rex_accept_intrinsic_pointer
end module rex_flang_intrinsic_module_type_provider

program rex_flang_intrinsic_module_type_identity
  use rex_flang_intrinsic_module_type_provider
  use iso_c_binding, only : c_null_ptr, c_ptr
  implicit none

  type(c_ptr) :: rex_direct_pointer

  rex_direct_pointer = c_null_ptr
  call rex_accept_intrinsic_pointer(rex_direct_pointer)
end program rex_flang_intrinsic_module_type_identity
