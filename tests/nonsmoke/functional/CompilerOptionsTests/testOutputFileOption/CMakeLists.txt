add_test(
  NAME compiler_options_output_file
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${ROSE_TEST_TRANSLATOR} -rose:verbose 0 -c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_bool.C -o my_test_bool.o
)

add_test(
  NAME nonsmoke_functional_CompilerOptionsTests_testOutputFileOption_test_bool.C
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testTranslator> -c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_bool.C -o my_test_bool.o
)
set_tests_properties(nonsmoke_functional_CompilerOptionsTests_testOutputFileOption_test_bool.C PROPERTIES DEPENDS compiler_options_output_file)

