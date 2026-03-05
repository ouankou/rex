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
  test2016_04.C
  test2016_05.C
  test2016_10.C
  test2016_15.C
  test2016_19.C
)
set(CXX14_DISABLED_TESTCODES
  test2016_01.C
  test2016_13.C
  test2016_17.C
  test2016_22.C
  test2017_01.C
  test2016_06.C
)

set(ROSE_FLAGS -w -rose:verbose 0 -std=c++14)

set(ROSE_DISABLED_TESTCODES ${CXX14_DISABLED_TESTCODES})
set(_cxx14_check_local_tests)

foreach(file_to_test ${CXX14_TESTCODES})
  compile_test(${file_to_test} "Cxx14_tests")
  rex_compile_test_name(_test_name Cxx14_tests "${file_to_test}")
  list(APPEND _cxx14_check_local_tests "${_test_name}")
endforeach()

# Makefile check targets.
list(REMOVE_DUPLICATES _cxx14_check_local_tests)
string(REPLACE ";" "\\;" _cxx14_check_local_tests_escaped "${_cxx14_check_local_tests}")

add_test(
  NAME nonsmoke_functional_CompileTests_Cxx14_tests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=${_cxx14_check_local_tests_escaped}
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(
  nonsmoke_functional_CompileTests_Cxx14_tests_check-local
  PROPERTIES DEPENDS "${_cxx14_check_local_tests}"
)
