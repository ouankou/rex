program rex_fortran_directive_producer_openacc
  implicit none
  integer :: rex_queue
  rex_queue = 1
!$acc wait(rex_queue + 1)
  continue
end program
