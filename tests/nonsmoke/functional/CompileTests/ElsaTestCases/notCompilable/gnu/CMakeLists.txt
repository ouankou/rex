include(ElsaTestCases_notCompilable_gnu_Testcodes.cmake)
include(ElsaTestCases_notCompilable_gnu_Testcodes_Disabled.cmake)

set(ROSE_FLAGS -w)
set(ROSE_DISABLED_TESTCODES
  ${ELSATEST_NOTCOMPILABLE_GNU_TESTCODES}
  ${ELSATEST_NOTCOMPILABLE_GNU_DISABLED_TESTCODES})

foreach(file_to_test ${ELSATEST_NOTCOMPILABLE_GNU_TESTCODES}
                     ${ELSATEST_NOTCOMPILABLE_GNU_DISABLED_TESTCODES})
  compile_test(${file_to_test} ELSATEST)
endforeach()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_ElsaTestCases_notCompilable_gnu_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=ELSATEST_d0082_cc\;ELSATEST_d0096_c\;ELSATEST_d0099_c\;ELSATEST_d0122_c\;ELSATEST_dC0002_c\;ELSATEST_dC0007_c\;ELSATEST_dC0014_c\;ELSATEST_g0009_c\;ELSATEST_g0015_cc\;ELSATEST_g0016_cc\;ELSATEST_g0019_cc\;ELSATEST_g0021_cc\;ELSATEST_g0031_cc\;ELSATEST_k0002_cc
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_ElsaTestCases_notCompilable_gnu_check-local PROPERTIES
  LABELS "disabled"
  DEPENDS "ELSATEST_d0082_cc;ELSATEST_d0096_c;ELSATEST_d0099_c;ELSATEST_d0122_c;ELSATEST_dC0002_c;ELSATEST_dC0007_c;ELSATEST_dC0014_c;ELSATEST_g0009_c;ELSATEST_g0015_cc;ELSATEST_g0016_cc;ELSATEST_g0019_cc;ELSATEST_g0021_cc;ELSATEST_g0031_cc;ELSATEST_k0002_cc"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_CompileTests_ElsaTestCases_notCompilable_gnu_check-local PROPERTIES DISABLED TRUE)
endif()
