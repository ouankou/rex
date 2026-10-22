program rex_openmp_ordered_cycle
  implicit none
  integer(kind=8) :: i, expected
  integer :: k

  expected = 20
  !$omp parallel do ordered schedule(dynamic, 2) private(k)
  work: do i = 20, 1, -1
    if (mod(i, 3_8) == 0) cycle work
    do k = 1, 2
      if (k == 1) cycle
    end do
    !$omp ordered
    do while (mod(expected, 3_8) == 0)
      expected = expected - 1
    end do
    if (i /= expected) error stop 1
    expected = expected - 1
    !$omp end ordered
  end do work
  !$omp end parallel do
  if (expected /= 0) error stop 2
end program rex_openmp_ordered_cycle
