program rex_fortran_openmp_allocate_user_semantic_positive
  use omp_lib, only: omp_allocator_handle_kind, omp_default_mem_alloc
  implicit none
  integer(omp_allocator_handle_kind) :: allocator
  integer :: value, total

  allocator = omp_default_mem_alloc
  total = 0
!$omp parallel private(value) shared(total) allocate(allocator : value)
  value = 1
!$omp atomic update
  total = total + value
!$omp end parallel
  print *, 'total', total
end program rex_fortran_openmp_allocate_user_semantic_positive
