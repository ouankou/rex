program rex_fortran_character_literal_kinds
  implicit none
  integer, parameter :: ucs4 = selected_char_kind('ISO_10646')
  character(kind=ucs4, len=2) :: value = ucs4_'éΩ'
  if (kind(value) /= ucs4) stop 1
  if (value /= ucs4_'éΩ') stop 2
  if (len(ucs4_'éΩ') /= 2) stop 3
  if (iachar(value(1:1)) /= 233) stop 4
  if (iachar(value(2:2)) /= 937) stop 5
end program
