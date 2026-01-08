function(rex_add_disabled_test)
  add_test(${ARGV})
  set(_name "")
  list(FIND ARGV "NAME" _name_index)
  if(_name_index GREATER -1)
    math(EXPR _name_index "${_name_index}+1")
    list(GET ARGV ${_name_index} _name)
  elseif(ARGC GREATER 0)
    set(_name "${ARGV0}")
  endif()
  if(_name)
    set_tests_properties(${_name} PROPERTIES LABELS "disabled")
    if(NOT REX_ENABLE_DISABLED_TESTS)
      set_tests_properties(${_name} PROPERTIES DISABLED TRUE)
    endif()
  endif()
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

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_RunTests_FortranTests_LANL_POP_netcdf-4.1.1_check_nc_config
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=netcdf_check_nc_config\;netcdf_test_prog_F\;netcdf_test_prog_c\;netcdf_test_prog_f90
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_RunTests_FortranTests_LANL_POP_netcdf-4.1.1_check_nc_config PROPERTIES
  LABELS "disabled"
  DEPENDS "netcdf_check_nc_config;netcdf_test_prog_F;netcdf_test_prog_c;netcdf_test_prog_f90"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_RunTests_FortranTests_LANL_POP_netcdf-4.1.1_check_nc_config PROPERTIES DISABLED TRUE)
endif()
