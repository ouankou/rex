add_test(
  NAME compiler_options_ansi
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 -ansi
    ${CMAKE_CURRENT_SOURCE_DIR}/test_c.c
)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompilerOptionsTests_testAnsiOption_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=compiler_options_ansi
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompilerOptionsTests_testAnsiOption_check-local PROPERTIES DEPENDS compiler_options_ansi )
