program rex_fortran_openmp_unknown_ompx_comment
  implicit none

!$ompx test_nonexistent
  print *, "unknown OMPX source line is preserved"
end program rex_fortran_openmp_unknown_ompx_comment
