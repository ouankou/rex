program rex_fortran_openmp_allocate_predefined_semantic_positive
  use omp_lib, only: omp_default_mem_alloc
  implicit none
  integer :: value, total

  total = 0
!$omp parallel private(value) shared(total) allocate(omp_default_mem_alloc : value)
  value = 1
!$omp atomic update
  total = total + value
!$omp end parallel
  print *, 'total', total
end program rex_fortran_openmp_allocate_predefined_semantic_positive
