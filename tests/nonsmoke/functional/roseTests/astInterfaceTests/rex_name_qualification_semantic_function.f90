program rex_name_qualification_semantic_function
  implicit none
  integer, external :: rex_twice

  if (rex_twice(3) /= 6) error stop 1
end program rex_name_qualification_semantic_function

integer function rex_twice(value)
  implicit none
  integer, intent(in) :: value

  rex_twice = value * 2
end function rex_twice
