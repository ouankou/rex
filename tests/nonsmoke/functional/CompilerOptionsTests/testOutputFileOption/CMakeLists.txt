add_test(
  NAME compiler_options_output_file
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 -c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_bool.C -o my_test_bool.o
)
