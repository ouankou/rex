program rex_fortran_openmp_threadprivate_common_member_copyprivate
  implicit none
  integer :: value, other
  common /state/ value, other
!$omp threadprivate(/state/)

!$omp parallel
!$omp single
  value = 42
!$omp end single copyprivate(value)
!$omp end parallel
end program rex_fortran_openmp_threadprivate_common_member_copyprivate
