module rex_left_types
contains
  function rex_make() result(rex_result)
    type rex_node
      integer :: rex_value
    end type rex_node
    type(rex_node) :: rex_result
    rex_result%rex_value = 17
  end function rex_make
  integer function rex_check()
    associate(rex_local => rex_make())
      rex_check = 0
      if (rex_local%rex_value /= 17) rex_check = 1
    end associate
  end function rex_check
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
  integer function rex_check()
    associate(rex_local => rex_make())
      rex_check = 0
      if (any(rex_local%rex_value /= [23, 29])) rex_check = 1
    end associate
  end function rex_check
end module rex_right_types

program rex_fortran_nominal_type_identity
  use rex_left_types, only: rex_left_check => rex_check
  use rex_right_types, only: rex_right_check => rex_check
  if (rex_left_check() /= 0) stop 1
  if (rex_right_check() /= 0) stop 2
end program rex_fortran_nominal_type_identity
