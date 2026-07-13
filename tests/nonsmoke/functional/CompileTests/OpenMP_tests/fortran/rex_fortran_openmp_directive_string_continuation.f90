subroutine rex_fortran_openmp_directive_string_continuation()
  implicit none

!$omp error at(compilation) severity(warning) message("this OpenMP directive &
!$omp& &string preserves ""doubled delimiters"" and an ! inside its literal &
!$omp& &while using the exact free-form character continuation contract")
end subroutine rex_fortran_openmp_directive_string_continuation
