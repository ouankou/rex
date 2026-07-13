program rex_fortran_openmp_allocate_predefined_oracle
  implicit none
  integer :: value, total

  total = 0
!$omp parallel private(value) shared(total)
  value = 1
!$omp atomic update
  total = total + value
!$omp end parallel
  print *, 'total', total
end program rex_fortran_openmp_allocate_predefined_oracle
