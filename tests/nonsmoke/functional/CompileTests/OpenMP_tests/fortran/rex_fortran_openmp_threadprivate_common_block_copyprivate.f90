program rex_fortran_openmp_threadprivate_common_block_copyprivate
  implicit none
  integer :: value, other
  common /state/ value, other
!$omp threadprivate(/state/)

!$omp parallel
!$omp single
  value = 42
  other = 24
!$omp end single copyprivate(/state/)
!$omp end parallel
end program rex_fortran_openmp_threadprivate_common_block_copyprivate
