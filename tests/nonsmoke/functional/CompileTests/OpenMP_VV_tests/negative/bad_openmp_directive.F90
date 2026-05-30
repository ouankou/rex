program bad_openmp_directive
  implicit none
  integer :: x

  x = 0
  !$omp definitely_not_a_directive
  x = x + 1
end program bad_openmp_directive
