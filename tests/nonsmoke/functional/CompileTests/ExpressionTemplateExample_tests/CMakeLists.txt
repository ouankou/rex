set(TESTCODES
  test2006_30.C
  test2006_31.C
  test2006_32.C
  test2006_29.C
)

set(ROSE_DISABLED_TESTCODES test2006_29.C)

foreach(file_to_test ${TESTCODES})
  compile_test(${file_to_test} ExpressionTemplateExample_tests)
endforeach()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_ExpressionTemplateExample_tests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=ExpressionTemplateExample_tests_test2006_29_C\;ExpressionTemplateExample_tests_test2006_30_C\;ExpressionTemplateExample_tests_test2006_31_C\;ExpressionTemplateExample_tests_test2006_32_C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_ExpressionTemplateExample_tests_check-local PROPERTIES
  DEPENDS "ExpressionTemplateExample_tests_test2006_29_C;ExpressionTemplateExample_tests_test2006_30_C;ExpressionTemplateExample_tests_test2006_31_C;ExpressionTemplateExample_tests_test2006_32_C"
)
