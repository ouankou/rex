program rex_fortran_signed_integer_minimum_values
  implicit none
  integer(1) :: minimum1 = -128_1
  integer(2) :: minimum2 = -32768_2
  integer(4) :: minimum4 = -2147483648_4
  integer(8) :: minimum8 = -9223372036854775808_8
  integer(16) :: minimum16 = -170141183460469231731687303715884105728_16
  if (minimum1 /= -huge(minimum1) - 1_1) stop 1
  if (minimum2 /= -huge(minimum2) - 1_2) stop 2
  if (minimum4 /= -huge(minimum4) - 1_4) stop 3
  if (minimum8 /= -huge(minimum8) - 1_8) stop 4
  if (minimum16 /= -huge(minimum16) - 1_16) stop 5
end program
