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

program main
  implicit none
  integer :: rex_fortran_openmp_fibonacci
  external rex_fortran_openmp_fibonacci
  integer(kind=1) :: small_index, small_values(3)
  integer(kind=8) :: wide_index, wide_values(3)

  print *, 'fib', rex_fortran_openmp_fibonacci(10)
  small_values = 0_1
  wide_values = 0_8
  do small_index = 1_1, 3_1
!$omp task shared(small_values)
    small_values(small_index) = small_index
!$omp end task
  end do
  do wide_index = 1099511627776_8, 1099511627778_8
!$omp task shared(wide_values)
    wide_values(wide_index - 1099511627775_8) = wide_index
!$omp end task
  end do
!$omp taskwait
  if (small_values(1) /= 1_1 .or. small_values(2) /= 2_1 .or. &
      small_values(3) /= 3_1) stop 1
  if (wide_values(1) /= 1099511627776_8 .or. &
      wide_values(2) /= 1099511627777_8 .or. &
      wide_values(3) /= 1099511627778_8) stop 2
end program main
