include(ElsaTestCases_notCompilable_std_Testcodes.cmake)
include(ElsaTestCases_notCompilable_std_Testcodes_Additional.cmake)

set(ROSE_FLAGS -w -std=gnu++03)
set(ELSATEST_NOTCOMPILABLE_STD_ALL_TESTCODES
  ${ELSATEST_NOTCOMPILABLE_STD_TESTCODES}
  ${ELSATEST_NOTCOMPILABLE_STD_ADDITIONAL_TESTCODES})

set(_elsa_notcompilable_std_roots
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../std
  ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

foreach(file_to_test ${ELSATEST_NOTCOMPILABLE_STD_ALL_TESTCODES})
  elsa_not_compilable_test(${file_to_test} ELSATEST
    ${_elsa_notcompilable_std_roots})
endforeach()

