# this defines ROSE__CXX_TESTS and ROSE__CXX_TESTS__BOOST
# include(${CMAKE_CURRENT_SOURCE_DIR}/Cxx_Testcodes_bk.cmake)

include(Cxx_Testcodes.cmake)
include(Cxx_Failing_Tests.cmake)

set(ROSE_FLAGS
  --edg:no_warnings -w -rose:verbose 0
  -rose:skip_unparse_asm_commands
  -rose:skip_unparse_cc_commands
  -I${CMAKE_CURRENT_SOURCE_DIR}/../A++Code
  )

if(REX_ENABLE_FAILING_TESTS)
  foreach(file_to_test ${TESTCODE_CURRENTLY_FAILING})
    compile_test(${file_to_test} "Cxx_tests_failing")
  endforeach()
endif()

foreach(file_to_test ${EXAMPLE_TESTCODES_REQUIRED_TO_PASS})
  compile_test(${file_to_test} "Cxx_tests")
endforeach()
