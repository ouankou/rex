subroutine rex_fortran_openacc_directive_context_coverage()
  implicit none
  integer :: loop_index
  integer :: cache_value_alpha(16), cache_value_beta(16), cache_value_gamma(16)
  integer :: cache_value_delta(16), cache_value_epsilon(16), cache_value_zeta(16)

!$acc wait(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)
!$acc parallel copy(cache_value_alpha, cache_value_beta, cache_value_gamma, cache_value_delta)
  do loop_index = 1, 16
!$acc cache(cache_value_alpha, cache_value_beta, cache_value_gamma, cache_value_delta)
    cache_value_alpha(loop_index) = cache_value_zeta(loop_index)
  end do
!$acc end parallel
end subroutine rex_fortran_openacc_directive_context_coverage
