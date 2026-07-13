#define REX_OMP_MESSAGE "MiXeD  directive  spacing"
program rex_fortran_openmp_macro_string_surface
  implicit none
!$OMP ERROR AT(EXECUTION) SEVERITY(WARNING) MESSAGE(REX_OMP_MESSAGE)
end program rex_fortran_openmp_macro_string_surface
