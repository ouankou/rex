program rex_fortran_openmp_taskgroup_end
  implicit none
  integer :: value

  value = 0
!$omp parallel shared(value)
!$omp single
!$omp taskgroup
!$omp task shared(value)
  value = value + 1
!$omp end task
!$omp end taskgroup
!$omp end single
!$omp end parallel

  if (value < 0) error stop
end program rex_fortran_openmp_taskgroup_end
