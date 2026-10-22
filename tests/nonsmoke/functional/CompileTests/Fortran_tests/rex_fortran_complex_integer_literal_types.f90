program rex_fortran_complex_integer_literal_types
  integer, parameter :: repeat_kind = 8
  integer :: numeric_repeat(2), named_repeat(2)
  integer(kind=16) :: positive = 170141183460469231731687303715884105727_16
  integer(kind=16) :: negative = -170141183460469231731687303715884105727_16
  complex :: pair = (170141183460469231731687303715884105727_16, &
                    -170141183460469231731687303715884105727_16)
  data numeric_repeat /2_8 *1/
  data named_repeat /2_repeat_kind *2/
  if (real(pair) <= 0.0 .or. aimag(pair) >= 0.0) stop 1
  if (positive <= 0 .or. negative >= 0) stop 2
  if (any(numeric_repeat /= 1) .or. any(named_repeat /= 2)) stop 3
end program
