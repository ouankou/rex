program rex_flang_openmp_callback_source
  implicit none
  ! These are ordinary character literals, never parsed OpenMP directives.
  character(*), parameter :: rex_parallel = '!$omp parallel'
  character(*), parameter :: rex_end_parallel = '!$omp end parallel'
  character(*), parameter :: rex_sections = '!$omp sections'
  character(*), parameter :: rex_end_sections = '!$omp end sections'
  character(*), parameter :: rex_simd = '!$omp declare simd'
  character(*), parameter :: rex_variant = '!$omp declare variant(rex_impl)'
  character(*), parameter :: rex_invalid = '!$omp rex_invalid'
  character(*), parameter :: rex_if = '!$omp parallel if(.true.)'
  character(*), parameter :: rex_acq_rel = '!$omp atomic acq_rel'
  integer :: rex_queue
  rex_queue = 1
end program
