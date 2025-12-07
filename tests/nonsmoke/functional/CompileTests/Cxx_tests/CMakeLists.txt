# this defines ROSE__CXX_TESTS and ROSE__CXX_TESTS__BOOST
# include(${CMAKE_CURRENT_SOURCE_DIR}/Cxx_Testcodes_bk.cmake)

include(Cxx_Testcodes.cmake)
include(Cxx_Failing_Tests.cmake)

set(CXX_TESTCODES_TO_RUN ${EXAMPLE_TESTCODES_REQUIRED_TO_PASS})
list(REMOVE_ITEM CXX_TESTCODES_TO_RUN ${TESTCODE_CURRENTLY_FAILING})

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

foreach(file_to_test ${CXX_TESTCODES_TO_RUN})
  compile_test(${file_to_test} "Cxx_tests")
endforeach()

add_test(
  NAME Cxx_tests_test_issue6_specialization
  COMMAND ${translator}
    -rose:verbose 0
    -rose:skipfinalCompileStep
    -c ${CMAKE_CURRENT_SOURCE_DIR}/test_issue6_specialization.cpp)
set_tests_properties(Cxx_tests_test_issue6_specialization PROPERTIES LABELS "Cxx_tests")
