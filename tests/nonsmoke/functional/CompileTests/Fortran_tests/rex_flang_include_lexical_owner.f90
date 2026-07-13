program rex_flang_include_lexical_owner
  implicit none
  integer :: rex_value
  integer :: rex_index

  rex_value = 0
  include "rex_flang_include_program.inc"
  if (rex_value > 0) then
    include "rex_flang_include_if.inc"
  end if
  do rex_index = 1, 2
    include "rex_flang_include_do.inc"
  end do
end program rex_flang_include_lexical_owner
