if(ENABLE-FORTRAN)
  add_subdirectory(fortran)
endif()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_OpenACC_tests_check-c
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=OpenACC_fortran_output_diff_trans_F90\;OpenACC_fortran_output_trans_F90\;OpenACC_fortran_trans.F90
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_OpenACC_tests_check-c PROPERTIES
  LABELS "disabled"
  DEPENDS "OpenACC_fortran_output_diff_trans_F90;OpenACC_fortran_output_trans_F90;OpenACC_fortran_trans.F90"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_CompileTests_OpenACC_tests_check-c PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_CompileTests_OpenACC_tests_check-cxx
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=OpenACC_fortran_output_diff_trans_F90\;OpenACC_fortran_output_trans_F90\;OpenACC_fortran_trans.F90
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_OpenACC_tests_check-cxx PROPERTIES
  LABELS "disabled"
  DEPENDS "OpenACC_fortran_output_diff_trans_F90;OpenACC_fortran_output_trans_F90;OpenACC_fortran_trans.F90"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_CompileTests_OpenACC_tests_check-cxx PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_CompileTests_OpenACC_tests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=OpenACC_fortran_output_diff_trans_F90\;OpenACC_fortran_output_trans_F90\;OpenACC_fortran_trans.F90
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_OpenACC_tests_check-local PROPERTIES
  DEPENDS "OpenACC_fortran_output_diff_trans_F90;OpenACC_fortran_output_trans_F90;OpenACC_fortran_trans.F90"
)
add_test(
  NAME nonsmoke_functional_CompileTests_OpenACC_tests_conditional-check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=OpenACC_fortran_output_diff_trans_F90\;OpenACC_fortran_output_trans_F90\;OpenACC_fortran_trans.F90
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_OpenACC_tests_conditional-check-local PROPERTIES
  DEPENDS "OpenACC_fortran_output_diff_trans_F90;OpenACC_fortran_output_trans_F90;OpenACC_fortran_trans.F90"
)
