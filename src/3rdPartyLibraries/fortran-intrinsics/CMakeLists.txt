if(ENABLE-FORTRAN)
  install(
    FILES
      iso_c_binding.f90
      iso_c_binding.rmod
      openacc.f90
      openacc.rmod
      omp_lib.f90
      omp_lib.rmod
      omp_lib_kinds.rmod
    DESTINATION share/rose)
endif()
