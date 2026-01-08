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

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_ElsaTestCases_notCompilable_std_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=ELSATEST_13_3_3_1_4_cc\;ELSATEST_13_3_3_2a_cc\;ELSATEST_13_3_3_2b_cc\;ELSATEST_13_3_3_2d_cc\;ELSATEST_13_3_3b_cc\;ELSATEST_13_cc\;ELSATEST_3_4_5_cc\;ELSATEST_7_3_1_cc\;ELSATEST_7_3_3e_cc\;ELSATEST_7_3_3g_cc\;ELSATEST_7_3_3h_cc\;ELSATEST_7_3_3i_cc\;ELSATEST_7_3_3j_cc\;ELSATEST_7_3_4a_cc\;ELSATEST_7_3_4c_cc
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_ElsaTestCases_notCompilable_std_check-local PROPERTIES
  LABELS "disabled"
  DEPENDS "ELSATEST_13_3_3_1_4_cc;ELSATEST_13_3_3_2a_cc;ELSATEST_13_3_3_2b_cc;ELSATEST_13_3_3_2d_cc;ELSATEST_13_3_3b_cc;ELSATEST_13_cc;ELSATEST_3_4_5_cc;ELSATEST_7_3_1_cc;ELSATEST_7_3_3e_cc;ELSATEST_7_3_3g_cc;ELSATEST_7_3_3h_cc;ELSATEST_7_3_3i_cc;ELSATEST_7_3_3j_cc;ELSATEST_7_3_4a_cc;ELSATEST_7_3_4c_cc"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_CompileTests_ElsaTestCases_notCompilable_std_check-local PROPERTIES DISABLED TRUE)
endif()
