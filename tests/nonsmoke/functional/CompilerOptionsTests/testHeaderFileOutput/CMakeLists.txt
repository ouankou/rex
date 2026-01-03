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
