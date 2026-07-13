module rex_flang_use_procedure_array_source_provider
  implicit none
contains

  function rex_scale_array(values) result(scaled)
    real, intent(in) :: values(0:)
    real :: scaled(0:size(values) - 1)

    scaled = 2.0 * values
  end function rex_scale_array

end module rex_flang_use_procedure_array_source_provider

program rex_flang_use_procedure_array_source
  use rex_flang_use_procedure_array_source_provider, only: &
      rex_local_scale => rex_scale_array
  implicit none

  real :: input(0:1)
  real :: output(0:1)
  real, allocatable :: workspace(:)

  input = [1.0, 2.0]
  output = rex_local_scale(input)
  allocate(workspace(0:1))
  workspace = output

  if (any(workspace /= [2.0, 4.0])) error stop 1
end program rex_flang_use_procedure_array_source
