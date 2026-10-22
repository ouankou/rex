program rex_fortran_openmp_if_forms
  implicit none
  integer :: value = 0
!$omp parallel if(.false.) shared(value)
  if (value < 0) then
    stop 1
  else if (value == 0) then
    value = 1
  else
    stop 2
  end if
  if (value /= 1) stop 3
!$omp end parallel
  if (value /= 1) stop 4
end program
