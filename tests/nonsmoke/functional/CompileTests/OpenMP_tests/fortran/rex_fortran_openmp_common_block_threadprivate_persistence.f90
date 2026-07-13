program rex_fortran_openmp_common_block_threadprivate_persistence
  use omp_lib, only: omp_get_thread_num
  implicit none
  integer :: thread_value
  integer :: thread_index
  integer :: first_values(64), second_values(64)
  common /rex_threadprivate_state/ thread_value
!$omp threadprivate(/rex_threadprivate_state/)

  thread_value = 10
  first_values = 0
  second_values = 0

!$omp parallel copyin(/rex_threadprivate_state/) private(thread_index) shared(first_values)
  thread_index = omp_get_thread_num() + 1
  thread_value = thread_value + thread_index
  first_values(thread_index) = thread_value
!$omp end parallel

!$omp parallel private(thread_index) shared(second_values)
  thread_index = omp_get_thread_num() + 1
  second_values(thread_index) = thread_value
!$omp end parallel

  print *, 'first', sum(first_values)
  print *, 'second', sum(second_values)
  print *, 'primary', thread_value
end program rex_fortran_openmp_common_block_threadprivate_persistence
