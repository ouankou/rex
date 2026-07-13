recursive integer function rex_fortran_openmp_fibonacci(number) result(value)
  implicit none
  integer, intent(in) :: number
  integer :: first, second

  if (number < 2) then
    value = number
  else
!$omp task shared(first)
    first = rex_fortran_openmp_fibonacci(number - 1)
!$omp end task
!$omp task shared(second)
    second = rex_fortran_openmp_fibonacci(number - 2)
!$omp end task
!$omp taskwait
    value = first + second
  end if
end function rex_fortran_openmp_fibonacci
