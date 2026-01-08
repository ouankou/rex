 # RiceCAF test lists are empty in Automake; no CTest entries needed yet.

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_Fortran_tests_RiceCAF_tests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
