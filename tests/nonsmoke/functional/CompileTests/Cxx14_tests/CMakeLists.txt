set(CXX14_TESTCODES
  test2016_01.C
  test2016_12.C
  test2016_13.C
  test2016_16.C
  test2016_17.C
  test2016_18.C
  test2016_20.C
  test2016_21.C
  test2016_22.C
  test2016_23.C
  test2017_01.C
  test2016_40.C
  test2016_06.C
  test2016_07.C
  test2016_08.C
  test2016_09.C
  test2016_14.C
  test2016_26.C
  test2018_01.C
  test2016_03.C
  test2016_11.C
  test2016_25.C
  test2016_02.C
  test2016_04.C
  test2016_05.C
  test2016_10.C
  test2016_15.C
  test2016_19.C
  test2016_24.C
)
set(CXX14_DISABLED_TESTCODES
  test2016_01.C
  test2016_12.C
  test2016_13.C
  test2016_16.C
  test2016_17.C
  test2016_18.C
  test2016_20.C
  test2016_21.C
  test2016_22.C
  test2016_23.C
  test2017_01.C
  test2016_40.C
  test2016_06.C
)

set(ROSE_FLAGS -w -rose:verbose 0 -std=c++14)

set(ROSE_DISABLED_TESTCODES ${CXX14_DISABLED_TESTCODES})

foreach(file_to_test ${CXX14_TESTCODES})
  compile_test(${file_to_test} "Cxx14_tests")
endforeach()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_Cxx14_tests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=Cxx14_tests_test2016_01_C\;Cxx14_tests_test2016_06_C\;Cxx14_tests_test2016_07_C\;Cxx14_tests_test2016_08_C\;Cxx14_tests_test2016_09_C\;Cxx14_tests_test2016_12_C\;Cxx14_tests_test2016_13_C\;Cxx14_tests_test2016_14_C\;Cxx14_tests_test2016_16_C\;Cxx14_tests_test2016_17_C\;Cxx14_tests_test2016_18_C\;Cxx14_tests_test2016_20_C\;Cxx14_tests_test2016_21_C\;Cxx14_tests_test2016_22_C\;Cxx14_tests_test2016_23_C\;Cxx14_tests_test2016_26_C\;Cxx14_tests_test2017_01_C\;Cxx14_tests_test2018_01_C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_Cxx14_tests_check-local PROPERTIES
  DEPENDS "Cxx14_tests_test2016_01_C;Cxx14_tests_test2016_06_C;Cxx14_tests_test2016_07_C;Cxx14_tests_test2016_08_C;Cxx14_tests_test2016_09_C;Cxx14_tests_test2016_12_C;Cxx14_tests_test2016_13_C;Cxx14_tests_test2016_14_C;Cxx14_tests_test2016_16_C;Cxx14_tests_test2016_17_C;Cxx14_tests_test2016_18_C;Cxx14_tests_test2016_20_C;Cxx14_tests_test2016_21_C;Cxx14_tests_test2016_22_C;Cxx14_tests_test2016_23_C;Cxx14_tests_test2016_26_C;Cxx14_tests_test2017_01_C;Cxx14_tests_test2018_01_C"
)
