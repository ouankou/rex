program rex_fortran_directive_producer_openmp
  implicit none
  integer :: rex_queue
  rex_queue = 1
!$omp parallel num_threads(rex_queue + 1)
  continue
!$omp end parallel
end program
