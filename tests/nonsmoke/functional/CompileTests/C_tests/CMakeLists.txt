# this defines ROSE__C_TESTS
# include(${CMAKE_CURRENT_SOURCE_DIR}/C_Testcodes_bk.cmake)

include(C_Testcodes.cmake)
include(C_Failing_Tests.cmake)

set(C_TESTCODES_TO_RUN ${TESTCODES_REQUIRED_TO_PASS})
list(REMOVE_ITEM C_TESTCODES_TO_RUN ${TESTCODE_CURRENTLY_FAILING})

if(REX_ENABLE_FAILING_TESTS)
  foreach(file_to_test ${TESTCODE_CURRENTLY_FAILING})
    compile_test(${file_to_test} "C_tests_failing")
  endforeach()
endif()

foreach(file_to_test ${C_TESTCODES_TO_RUN})
  compile_test(${file_to_test} "C_tests")
endforeach()
