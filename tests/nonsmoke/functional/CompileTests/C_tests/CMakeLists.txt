# this defines ROSE__C_TESTS
# include(${CMAKE_CURRENT_SOURCE_DIR}/C_Testcodes_bk.cmake)

include(C_Testcodes.cmake)
include(C_Failing_Tests.cmake)

if(REX_ENABLE_FAILING_TESTS)
  foreach(file_to_test ${TESTCODE_CURRENTLY_FAILING})
    compile_test(${file_to_test} "C_tests_failing")
  endforeach()
endif()

if(TESTCODE_CURRENTLY_FAILING)
  list(REMOVE_ITEM TESTCODES_REQUIRED_TO_PASS ${TESTCODE_CURRENTLY_FAILING})
endif()

foreach(file_to_test ${TESTCODES_REQUIRED_TO_PASS})
  compile_test(${file_to_test} "C_tests")
endforeach()
