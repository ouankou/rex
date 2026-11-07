! REX_UNIT_BEFORE
program rex_unit_procedure
integer total
call rex_set_total(total)
print '(I0)', total
end program rex_unit_procedure
subroutine rex_set_total(value) ! REX_UNIT_HEADER
integer value
! REX_UNIT_INSIDE
include "rex_flang_unit_procedure_end.inc"
! REX_UNIT_AFTER
