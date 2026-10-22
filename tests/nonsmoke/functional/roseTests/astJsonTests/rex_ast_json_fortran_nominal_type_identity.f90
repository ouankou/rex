module rex_left_types
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
