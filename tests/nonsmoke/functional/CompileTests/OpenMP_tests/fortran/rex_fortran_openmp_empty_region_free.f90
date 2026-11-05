program rex_fortran_openmp_empty_region_free
  implicit none
  integer :: value
  value = 7
! REX_EMPTY_BEFORE
!$omp parallel
!$omp end parallel
!$omp parallel ! REX_EMPTY_BEGIN_INLINE
! REX_EMPTY_INSIDE_FIRST

! REX_EMPTY_INSIDE_LAST
!$omp end parallel ! REX_EMPTY_END_INLINE
! REX_EMPTY_AFTER
  if (value /= 7) stop 1
  print *, value
end program
