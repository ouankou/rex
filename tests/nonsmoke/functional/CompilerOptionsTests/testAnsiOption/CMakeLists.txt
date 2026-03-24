add_test(
  NAME compiler_options_ansi
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 -ansi
    ${CMAKE_CURRENT_SOURCE_DIR}/test_c.c
)

