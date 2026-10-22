program rex_flang_prescan_source
  implicit none
  integer :: value
  value = 0
! Flang diagnoses and ignores trailing preprocessor tokens. They must never
! become executable Fortran through a source-rewriting adapter.
#include "rex_flang_prescan_empty.inc" ; value = 7
  if (value /= 0) stop 1
end program
