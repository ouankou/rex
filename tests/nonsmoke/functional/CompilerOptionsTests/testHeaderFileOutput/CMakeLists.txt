add_test(
  NAME compiler_options_header_file_output
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 -H -c
    ${CMAKE_CURRENT_SOURCE_DIR}/testHeaderFiles.C
)

add_test(
  NAME compiler_options_header_file_output_include_order
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 -c
    -I${CMAKE_CURRENT_SOURCE_DIR}/myinclude
    ${CMAKE_CURRENT_SOURCE_DIR}/test2009_515.c
)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompilerOptionsTests_testHeaderFileOutput_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=compiler_options_header_file_output\;compiler_options_header_file_output_include_order
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompilerOptionsTests_testHeaderFileOutput_check-local PROPERTIES DEPENDS "compiler_options_header_file_output;compiler_options_header_file_output_include_order" )
