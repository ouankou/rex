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
