program rex_entry_inline
  implicit none
  real, external :: rex_original, rex_alternate
  if (rex_original() /= 3.0) stop 1
  if (rex_alternate() /= 4.0) stop 2
  print '(I0)', 7
end program rex_entry_inline

recursive function rex_original() result(value) ! REX_PROCEDURE_HEADER_COMMENT
  real :: value
  value = 3.0
  return
  entry rex_alternate() result(value) ! REX_ENTRY_INLINE_COMMENT
  value = 4.0
end function rex_original ! REX_PROCEDURE_END_COMMENT
