set(TESTCODES
  test2006_30.C
  test2006_31.C
  test2006_32.C
  test2006_29.C
)

foreach(file_to_test ${TESTCODES})
  compile_test(${file_to_test} ExpressionTemplateExample_tests)
  if(REX_EXTENDED_TEST_TIMEOUTS)
    rex_compile_test_name(_test_name ExpressionTemplateExample_tests
                          ${file_to_test})
    set_tests_properties(${_test_name} PROPERTIES
      TIMEOUT 10800
      PROCESSORS 16)
  endif()
endforeach()
