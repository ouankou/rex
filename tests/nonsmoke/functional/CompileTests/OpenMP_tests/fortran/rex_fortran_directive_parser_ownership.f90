program directive_parser_ownership
  integer :: total

!$omp parallel shared(total)
!$omp do collapse(2) &
!$omp& private(i, j) lastprivate(total)
  do j = 1, 2
    do i = 1, 3
      total = i + j
    end do
  end do
!$omp end do
!$omp single
  print *, total
!$omp end single
!$omp end parallel
end program directive_parser_ownership
