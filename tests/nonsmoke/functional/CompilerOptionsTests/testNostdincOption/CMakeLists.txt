add_test(
  NAME compiler_options_nostdinc
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 -nostdinc
    ${CMAKE_CURRENT_SOURCE_DIR}/test.C
)
