module rex_fortran_openmp_directive_context_coverage
  implicit none
  integer :: threadprivate_value_alpha, threadprivate_value_beta
  integer :: threadprivate_value_gamma, threadprivate_value_delta
!$omp threadprivate(threadprivate_value_alpha, threadprivate_value_beta, threadprivate_value_gamma, threadprivate_value_delta)
contains
subroutine exercise_openmp_directive_context()
  integer :: loop_index
  integer :: payload_alpha, payload_beta, payload_gamma
  integer :: payload_delta, payload_epsilon

!$omp flush(payload_alpha, payload_beta, payload_gamma, payload_delta, payload_epsilon)
!$omp parallel do private(loop_index) shared(payload_alpha, payload_beta, payload_gamma, payload_delta)
  do loop_index = 1, 4
    payload_alpha = loop_index
  end do
!$omp end parallel do

!$omp parallel private(payload_alpha, payload_beta, payload_gamma, payload_delta)
!$omp single
  payload_beta = payload_alpha
!$omp end single copyprivate(payload_alpha, payload_beta, payload_gamma, payload_delta)
!$omp end parallel

end subroutine exercise_openmp_directive_context
end module rex_fortran_openmp_directive_context_coverage
