module rex_flang_include_middle
  use rex_flang_include_base, only: rex_payload
  implicit none
  type :: rex_wrapper
    type(rex_payload) :: value
  end type
contains
  function rex_make_value() result(result_value)
    type(rex_wrapper) :: result_value
    result_value%value%number = 7
  end function
end module
