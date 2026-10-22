module rex_left_types
  type rex_shared
    integer :: rex_value
  end type rex_shared
  type(rex_shared) :: rex_shared_value
contains
  function rex_make() result(rex_result)
    type rex_node
      integer :: rex_value
    end type rex_node
    type(rex_node) :: rex_result
    rex_result%rex_value = 17
  end function rex_make
end module rex_left_types

module rex_right_types
contains
  function rex_make() result(rex_result)
    type rex_node
      integer :: rex_value(2)
    end type rex_node
    type(rex_node) :: rex_result
    rex_result%rex_value = [23, 29]
  end function rex_make
end module rex_right_types

program rex_fortran_nominal_type_identity
  use rex_left_types, only: rex_left => rex_make, &
    local_type => rex_shared, imported_value => rex_shared_value
  use rex_right_types, only: rex_right => rex_make
  type(local_type) :: local_value
  imported_value%rex_value = 31
  local_value = imported_value
  if (local_value%rex_value /= 31) stop 3
  associate(rex_a => rex_left(), rex_b => rex_right())
    if (rex_a%rex_value /= 17) stop 1
    if (any(rex_b%rex_value /= [23, 29])) stop 2
  end associate
end program rex_fortran_nominal_type_identity
