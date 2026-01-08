compile_test(test2001_11.C AXXTEST)

# Makefile check targets.
set(_axx_group_tests AXXTEST_test2001_11_C)
add_test(
  NAME nonsmoke_functional_CompileTests_A++Tests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=${_axx_group_tests}
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_A++Tests_check-local PROPERTIES
  DEPENDS "${_axx_group_tests}"
)
