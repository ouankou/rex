program rex_process_type_roots
  implicit none
  if (rex_value() /= 7) stop 1
contains
  integer function rex_value()
    rex_value = 7
  end function rex_value
end program rex_process_type_roots
