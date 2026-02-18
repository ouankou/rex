if(ENABLE-FORTRAN)
  configure_file(omp_lib.f90.in ${CMAKE_CURRENT_BINARY_DIR}/omp_lib.f90 @ONLY)
  configure_file(omp_lib.rmod.in ${CMAKE_CURRENT_BINARY_DIR}/omp_lib.rmod @ONLY)
  install(
    FILES
      iso_c_binding.f90
      iso_c_binding.rmod
      openacc.f90
      openacc.rmod
      ${CMAKE_CURRENT_BINARY_DIR}/omp_lib.f90
      ${CMAKE_CURRENT_BINARY_DIR}/omp_lib.rmod
      omp_lib_kinds.rmod
    DESTINATION share/rose)
endif()
