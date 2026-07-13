module iso_fortran_env
  implicit none
  logical :: rex_shadow_value
end module iso_fortran_env

subroutine rex_use_intrinsic_module_shadow
  use, intrinsic :: iso_fortran_env, only : int8
  implicit none
  integer(kind=int8) :: rex_intrinsic_value

  rex_intrinsic_value = 0_int8
end subroutine rex_use_intrinsic_module_shadow

subroutine rex_use_nonintrinsic_module_shadow
  use, non_intrinsic :: iso_fortran_env, only : rex_shadow_value
  implicit none
  logical :: rex_shadow_copy

  rex_shadow_copy = rex_shadow_value
end subroutine rex_use_nonintrinsic_module_shadow
