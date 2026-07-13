set(REX_GFORTRAN_PATH_QUALIFIED_TEST_SPECIMENS
  contained_3.f90
  do_1.f90
  enum_1.f90
  implicit_2.f90)

function(rex_gfortran_suite_test_name output_variable suite_path input_file)
  if(input_file IN_LIST REX_GFORTRAN_PATH_QUALIFIED_TEST_SPECIMENS)
    set(_test_name "GFORTRANSUITE_${suite_path}_${input_file}")
  else()
    set(_test_name "GFORTRANSUITE_${input_file}")
  endif()
  set(${output_variable} "${_test_name}" PARENT_SCOPE)
endfunction()

add_subdirectory(gfortran.dg)
add_subdirectory(gfortran.fortran-torture)

rex_assert_unique_ctest_names_across_directories(
  "gfortran.dg and gfortran.fortran-torture suites"
  "${CMAKE_CURRENT_BINARY_DIR}/gfortran.dg"
  "${CMAKE_CURRENT_BINARY_DIR}/gfortran.fortran-torture")
