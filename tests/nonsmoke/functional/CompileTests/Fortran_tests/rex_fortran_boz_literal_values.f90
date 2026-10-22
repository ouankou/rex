program rex_fortran_boz_literal_values
  implicit none
  integer(8) :: wide
  integer(16) :: widest
  integer :: binary_value, octal_value, zero
  data wide / z'ffffffffffffffff' /
  data widest / z'fedcba98765432100123456789abcdef' /
  data binary_value / b'101101' /
  data octal_value / o'755' /
  data zero / z'0' /
  if (wide /= -1_8) stop 1
  if (wide /= int(z'ffffffffffffffff', kind=8)) stop 2
  if (widest /= int(z'fedcba98765432100123456789abcdef', kind=16)) stop 3
  if (binary_value /= 45) stop 4
  if (binary_value /= int(b'101101')) stop 5
  if (octal_value /= 493) stop 6
  if (octal_value /= int(o'755')) stop 7
  if (zero /= int(z'0')) stop 8
end program
