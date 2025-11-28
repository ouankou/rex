add_subdirectory(ctests)
add_subdirectory(gnu)
add_subdirectory(kandr)
add_subdirectory(std)

include(ElsaTestCases_Testcodes.cmake)
include(ElsaTestCases_Failing_Tests.cmake)

set(ROSE_FLAGS --edg:no_warnings -w)

foreach(file_to_test ${TESTCODES})
  compile_test(${file_to_test} ELSATEST)
endforeach()

if(REX_ENABLE_FAILING_TESTS)
  foreach(file_to_test ${START_OF_FAILED_TESTS_USING_ROSE})
    compile_test(${file_to_test} "ElsaTestCases_failing")
  endforeach()
  foreach(file_to_test ${START_OF_FAILED_TESTS_USING_ROSE_WITH_GNU_VER_4_X})
    compile_test(${file_to_test} "ElsaTestCases_failing")
  endforeach()
endif()
