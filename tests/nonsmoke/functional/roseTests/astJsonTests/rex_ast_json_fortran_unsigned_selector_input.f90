program rex_ast_json_fortran_unsigned_selector
  implicit none
  integer, parameter :: source_kind = kind(0)
  unsigned(kind=source_kind + 0) :: explicit_unsigned
  unsigned*4 :: star_unsigned
end program rex_ast_json_fortran_unsigned_selector
