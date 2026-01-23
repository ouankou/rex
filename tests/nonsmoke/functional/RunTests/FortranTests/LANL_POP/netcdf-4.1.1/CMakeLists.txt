function(rex_add_disabled_test)
  add_test(${ARGV})
endfunction()

add_subdirectory(cxx)
add_subdirectory(cxx4)
add_subdirectory(examples)
add_subdirectory(f90)
add_subdirectory(libcf)
add_subdirectory(libncdap3)
add_subdirectory(libncdap4)
add_subdirectory(libsrc4)
add_subdirectory(ncdap_test)
add_subdirectory(ncdump)
add_subdirectory(ncgen)
add_subdirectory(ncgen3)
add_subdirectory(nc_test4)
add_subdirectory(nctest)
add_subdirectory(nf_test)
add_subdirectory(udunits)

rex_add_disabled_test(
  NAME netcdf_check_nc_config
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ./test_prog
)

rex_add_disabled_test(
  NAME netcdf_test_prog_c
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND sh -c
    "${CMAKE_C_COMPILER} `./nc-config --cflags` ${CMAKE_CURRENT_SOURCE_DIR}/test_prog.c -o test_prog `./nc-config --libs`"
)

rex_add_disabled_test(
  NAME netcdf_test_prog_F
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND sh -c
    "${CMAKE_Fortran_COMPILER} `./nc-config --fflags` ${CMAKE_CURRENT_SOURCE_DIR}/test_prog.F -o test_prog `./nc-config --libs`"
)

rex_add_disabled_test(
  NAME netcdf_test_prog_f90
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND sh -c
    "${CMAKE_Fortran_COMPILER} `./nc-config --fflags` ${CMAKE_CURRENT_SOURCE_DIR}/test_prog.f90 -o test_prog `./nc-config --libs`"
)
