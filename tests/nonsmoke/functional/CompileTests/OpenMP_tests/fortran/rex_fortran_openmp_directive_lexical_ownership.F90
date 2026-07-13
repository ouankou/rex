#include "rex_fortran_openmp_directive_lexical_ownership.inc"

program rex_fortran_openmp_directive_lexical_ownership
  use rex_directive_owner_mod
  implicit none
  print *, rex_before + rex_after
end program rex_fortran_openmp_directive_lexical_ownership
