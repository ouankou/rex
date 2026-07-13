program rex_flang_openmp_runtime_include_semantics
  implicit none
  include "omp_lib.h"

  integer(omp_integer_kind) :: rex_threads
  rex_threads = omp_get_max_threads()
  print *, rex_threads
end program rex_flang_openmp_runtime_include_semantics
