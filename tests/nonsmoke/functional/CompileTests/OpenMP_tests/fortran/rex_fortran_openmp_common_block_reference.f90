subroutine openmp_common_block_reference()
  integer :: flag_a, flag_b
  common /FlAgS/ flag_a, flag_b
!$omp threadprivate(/fLaGs/)
!$omp parallel copyin(/FlAgS/)
  flag_a = flag_b
!$omp end parallel
end subroutine openmp_common_block_reference
