program rex_flang_include_main
  use rex_flang_include_middle, only: rex_wrapper, rex_make_value
  implicit none
  type(rex_wrapper) :: value
  integer(kind=1), target :: bytes(6)
  integer(kind=1) :: matrix(3,4)
  integer(kind=1), pointer :: pointer_bytes(:)
  character(len=2) :: data_first, data_second
  data data_first, data_second / 'abcdef'(2:3), 'abcdef'(3:4) /
  value = rex_make_value()
  if (value%value%number /= 7) stop 1
  bytes = 0
  bytes(1) = 1
  bytes(2:4) = 2
  if (sum(bytes) /= 7) stop 2
  matrix = 0
  matrix(1,2) = 1
  if (sum(matrix) /= 1) stop 3
  pointer_bytes => bytes(1:6:2)
  pointer_bytes(1) = 3
  if (bytes(1) /= 3) stop 4
  if (data_first /= 'bc' .or. data_second /= 'cd') stop 5
  if ('abcdef'(len('ab'(:)):len("abc"(:))) /= 'bc') stop 6
  if ('a''bc'(:) /= "a'bc") stop 7
  if ("abcdef"(2:) /= 'bcdef') stop 8
  if ('abcdef'(:2) /= 'ab') stop 9
  if (4_'abcd'(2:) /= 4_'bcd') stop 10
  if (4_'éΩ'(1:1) /= 4_'é') stop 11
end program
