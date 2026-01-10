add_test(
  NAME compiler_options_include_options
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${ROSE_TEST_TRANSLATOR} -rose:verbose 0
    -isystem ${CMAKE_CURRENT_SOURCE_DIR}/system_include
    -I${CMAKE_CURRENT_SOURCE_DIR}/include
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test_include.c
)

add_test(
  NAME nonsmoke_functional_CompilerOptionsTests_testIncludeOptions_test_include.c
  COMMAND $<TARGET_FILE:testTranslator> -rose:verbose 0
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test_include.c
)
set_tests_properties(nonsmoke_functional_CompilerOptionsTests_testIncludeOptions_test_include.c PROPERTIES DEPENDS compiler_options_include_options)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompilerOptionsTests_testIncludeOptions_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=compiler_options_include_options
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompilerOptionsTests_testIncludeOptions_check-local PROPERTIES DEPENDS compiler_options_include_options )
