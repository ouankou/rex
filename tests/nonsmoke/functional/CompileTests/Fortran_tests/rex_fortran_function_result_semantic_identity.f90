module rex_result_identity_provider
  implicit none

  type :: source_result_type
    integer :: payload
  end type source_result_type
end module rex_result_identity_provider

module rex_result_identity_consumer
  use rex_result_identity_provider, only: renamed_result_type => source_result_type
  implicit none

  integer :: source_result_type = -1

contains

  type(renamed_result_type) function rex_host_use_result(value) result(output)
    integer, intent(in) :: value

    output%payload = value
  end function rex_host_use_result

  function rex_local_renamed_use_result(value) result(output)
    use rex_result_identity_provider, only: local_result_type => source_result_type
    integer, intent(in) :: value
    type(local_result_type) :: output

    output%payload = value + source_result_type + 1
  end function rex_local_renamed_use_result
end module rex_result_identity_consumer

program rex_function_result_semantic_identity
  use rex_result_identity_consumer, only: renamed_result_type, &
      rex_host_use_result, rex_local_renamed_use_result
  implicit none

  type(renamed_result_type) :: first
  type(renamed_result_type) :: second

  first = rex_host_use_result(7)
  second = rex_local_renamed_use_result(11)
  if (first%payload /= 7 .or. second%payload /= 11) error stop 1
end program rex_function_result_semantic_identity
