module rex_generic_integer
  implicit none
contains
  integer function evaluate(value)
    integer, intent(in) :: value
    evaluate = value + 7
  end function
end module
module rex_generic_real
  implicit none
contains
  integer function evaluate(value)
    real, intent(in) :: value
    evaluate = int(value) + 11
  end function
end module
module rex_generic_api
  use rex_generic_integer, only: integer_evaluate => evaluate
  use rex_generic_real, only: real_evaluate => evaluate
  implicit none
  interface apply
    procedure integer_evaluate, real_evaluate
    module procedure apply
  end interface
contains
  integer function apply(value)
    double precision, intent(in) :: value
    apply = int(value) + 13
  end function
end module
program rex_generic_caller
  use rex_generic_api, only: apply
  implicit none
  if (apply(3) /= 10) stop 1
  if (apply(3.0) /= 14) stop 2
  if (apply(3.0d0) /= 16) stop 3
end program
