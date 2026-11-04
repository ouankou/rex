program rex_fortran_lexical_keywords
  implicit none
  integer, parameter :: device = 1, host = 2
  integer :: i, j, total
  total = 0
! rex_fortran_lexical_inactive_begin_keywords
#if 0
this inactive source is intentionally not valid Fortran
!$omp barrier
!$ompx rex_fortran_lexical_inactive_keywords_opaque
#endif
! rex_fortran_lexical_inactive_end_keywords
!$omp interchange permutation(device, host)
  do i = 1, 2
    do j = 1, 2
      total = total + 1
    end do
  end do
  if (total /= 4) stop 1
  print '(I0)', total
end program rex_fortran_lexical_keywords
