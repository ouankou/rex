add_test(
  NAME compiler_options_include_options
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${ROSE_TEST_TRANSLATOR} -rose:verbose 0
    -isystem ${CMAKE_CURRENT_SOURCE_DIR}/system_include
    -I${CMAKE_CURRENT_SOURCE_DIR}/include
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test_include.c
)
