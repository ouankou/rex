add_subdirectory(ctests)
add_subdirectory(big)
add_subdirectory(gnu)
add_subdirectory(kandr)
add_subdirectory(notCompilable)
add_subdirectory(std)

include(ElsaTestCases_Testcodes.cmake)
include(ElsaTestCases_Additional_Testcodes.cmake)

set(ROSE_FLAGS  -w -std=gnu++03)

set(ELSA_TESTCODES_ADDITIONAL_TO_RUN
  ${ELSATEST_ADDITIONAL_TESTCODES}
  ${ELSATEST_GNU4X_ADDITIONAL_TESTCODES}
)
list(REMOVE_DUPLICATES ELSA_TESTCODES_ADDITIONAL_TO_RUN)

set(ELSA_TESTCODES_TO_RUN
  ${TESTCODES}
  ${ELSA_TESTCODES_ADDITIONAL_TO_RUN}
)
list(REMOVE_DUPLICATES ELSA_TESTCODES_TO_RUN)

set(ELSATEST_EXPECT_FAIL
  t0071.cc
  t0253.cc
  t0337.cc
  t0373.cc
  t0460.cc
  t0461.cc
  t0464.cc
  t0469.cc
  t0470.cc
  t0482.cc
)

foreach(file_to_test ${ELSA_TESTCODES_TO_RUN})
  if(file_to_test IN_LIST ELSATEST_EXPECT_FAIL)
    compile_negative_test(${file_to_test} ELSATEST)
  else()
    compile_test(${file_to_test} ELSATEST)
  endif()
endforeach()

add_test(
  NAME nonsmoke_functional_CompileTests_ElsaTestCases_t0001.cc
  COMMAND testTranslator ${ROSE_FLAGS}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/t0001.cc
)
set_tests_properties(nonsmoke_functional_CompileTests_ElsaTestCases_t0001.cc PROPERTIES LABELS ELSATEST DEPENDS ELSATEST_t0001_cc)

