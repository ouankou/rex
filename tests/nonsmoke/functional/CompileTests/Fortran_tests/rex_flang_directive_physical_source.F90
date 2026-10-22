#line 900 "rex_logical_directive.f90"
program rex_flang_directive_physical_source
  integer :: index
  integer :: total
  total = 0
!dir$ unroll
  do index = 1, 2
    total = total + index
  end do
end program
