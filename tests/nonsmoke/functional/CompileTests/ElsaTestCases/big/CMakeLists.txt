include(ElsaTestCases_big_Testcodes.cmake)
include(ElsaTestCases_big_Testcodes_Disabled.cmake)

set(ROSE_FLAGS -w)
set(ROSE_DISABLED_TESTCODES ${ELSATEST_BIG_DISABLED_TESTCODES})

foreach(file_to_test ${ELSATEST_BIG_TESTCODES} ${ELSATEST_BIG_DISABLED_TESTCODES})
  compile_test(${file_to_test} ELSATEST)
endforeach()
