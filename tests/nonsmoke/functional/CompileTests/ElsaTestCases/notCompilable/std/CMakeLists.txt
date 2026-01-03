include(ElsaTestCases_notCompilable_std_Testcodes.cmake)
include(ElsaTestCases_notCompilable_std_Testcodes_Disabled.cmake)

set(ROSE_FLAGS -w)
set(ROSE_DISABLED_TESTCODES
  ${ELSATEST_NOTCOMPILABLE_STD_TESTCODES}
  ${ELSATEST_NOTCOMPILABLE_STD_DISABLED_TESTCODES})

foreach(file_to_test ${ELSATEST_NOTCOMPILABLE_STD_TESTCODES}
                     ${ELSATEST_NOTCOMPILABLE_STD_DISABLED_TESTCODES})
  compile_test(${file_to_test} ELSATEST)
endforeach()
