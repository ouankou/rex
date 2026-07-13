program rex_fortran_openmp_requires_dynamic_allocators_type_oracle
  implicit none
  integer, allocatable :: values(:)
  integer :: index

  allocate(values(8))
  values = [(index, index = 1, 8)]
  print *, 'sum', sum(values)
  deallocate(values)
end program rex_fortran_openmp_requires_dynamic_allocators_type_oracle
