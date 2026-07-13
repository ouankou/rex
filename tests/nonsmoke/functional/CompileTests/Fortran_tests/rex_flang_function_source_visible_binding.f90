module rex_function_binding_semantic_publication_module
  implicit none
contains
  function rex_function_binding_dynamic(values) result(text)
    integer, intent(in) :: values(1)
    character(len=rex_function_binding_length(values) + lbound(values, 1) - 1) :: text

    text = 'abc'
  end function rex_function_binding_dynamic

  pure integer function rex_function_binding_length(values)
    integer, intent(in) :: values(1)

    rex_function_binding_length = values(1)
  end function rex_function_binding_length
end module rex_function_binding_semantic_publication_module

recursive integer function rex_function_binding_factorial(n) result(value)
  integer, intent(in) :: n

  if (n <= 1) then
    value = 1
  else
    value = n * rex_function_binding_factorial(n - 1)
  end if
end function rex_function_binding_factorial

subroutine rex_function_binding_intrinsic(value)
  integer, intent(out) :: value
  real :: abs
  intrinsic :: abs

  value = abs(-2)
end subroutine rex_function_binding_intrinsic

program rex_function_source_visible_binding
  use rex_function_binding_semantic_publication_module
  integer :: lengths(1)

  interface
    recursive integer function rex_function_binding_factorial(n) result(value)
      integer, intent(in) :: n
    end function rex_function_binding_factorial
  end interface

  lengths = 3
  if (rex_function_binding_dynamic(lengths) /= 'abc') error stop 1
  print *, rex_function_binding_factorial(5)
end program rex_function_source_visible_binding
