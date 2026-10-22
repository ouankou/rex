module rex_semantic_component_module
  implicit none

  type :: rex_payload
    integer :: value
  end type rex_payload
contains
  subroutine rex_increment(item, amount)
    type(rex_payload), intent(inout) :: item
    integer, intent(in) :: amount
    item%value = item%value + amount
  end subroutine rex_increment
end module rex_semantic_component_module

program rex_fortran_semantic_name_and_component_identity
  use rex_semantic_component_module, only: rex_payload, rex_increment
  use iso_c_binding, only: c_int
  implicit none
  type, bind(c) :: rex_c_payload
    integer(c_int) :: native_integer
  end type
  type(rex_payload) :: payload
  type(rex_c_payload) :: c_payload
  integer :: amount
  complex :: complex_value

  amount = 4
  c_payload%native_integer = 11_c_int
  if (c_payload%native_integer /= 11_c_int) error stop
  payload = rex_payload(0)
  payload%value = 3
  complex_value = (1.0, 2.0)
  call rex_increment(payload, amount)
  if (payload%value /= 7) error stop
end program rex_fortran_semantic_name_and_component_identity
