program rex_fortran_unsigned_literal_values
  implicit none
  unsigned(1) :: u1 = 255u_1
  unsigned(2) :: u2 = 65535u_2
  unsigned(4) :: u4 = 4294967295u_4
  unsigned(8) :: u8 = 18446744073709551615u_8
  unsigned(1) :: wrapped = 256u_1
  unsigned(8) :: negated = -1u_8
  if (u1 /= 255u_1) stop 1
  if (u2 /= 65535u_2) stop 2
  if (u4 /= 4294967295u_4) stop 3
  if (u8 /= 18446744073709551615u_8) stop 4
  if (wrapped /= 0u_1) stop 5
  if (negated /= u8) stop 6
end program
