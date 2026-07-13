program rex_fortran_openmp_copyprivate_semantic_positive
  implicit none
  integer :: last_index, last_value

!$omp parallel private(last_index, last_value)
!$omp single
  last_index = 1
  last_value = 2
!$omp end single copyprivate(last_index, last_value)
!$omp end parallel
end program rex_fortran_openmp_copyprivate_semantic_positive
