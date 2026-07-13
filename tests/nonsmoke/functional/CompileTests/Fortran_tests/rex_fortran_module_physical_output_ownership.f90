module rex_fortran_module_physical_output_ownership
  implicit none
  integer :: exact_value = 7
contains
  subroutine set_exact_value(value)
    integer, intent(in) :: value
    exact_value = value
  end subroutine set_exact_value
end module rex_fortran_module_physical_output_ownership
