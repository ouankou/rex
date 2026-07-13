module rex_flang_use_overloaded_procedure_provider
  implicit none

  interface rex_scale
    module procedure rex_scale_integer
    module procedure rex_scale_real
  end interface rex_scale

contains

  integer function rex_scale_integer(value) result(scaled)
    integer, intent(in) :: value
    scaled = 2 * value
  end function rex_scale_integer

  real function rex_scale_real(value) result(scaled)
    real, intent(in) :: value
    scaled = 2.0 * value
  end function rex_scale_real

end module rex_flang_use_overloaded_procedure_provider

program rex_flang_use_overloaded_procedure_identity
  use rex_flang_use_overloaded_procedure_provider, only: &
      rex_local_scale => rex_scale
  implicit none

  integer :: integer_value
  real :: real_value

  integer_value = rex_local_scale(3)
  real_value = rex_local_scale(4.0)
  if (integer_value /= 6 .or. real_value /= 8.0) error stop 1
end program rex_flang_use_overloaded_procedure_identity
