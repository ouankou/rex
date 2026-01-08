set(C11_TESTCODES
  test2014_01.c
  test2014_02.c
  test2014_04.c
  test2014_05.c
  test2014_03.c
  test2014_06.c
  test2014_07.c
  test2014_08.c
  test2014_09.c
  test2014_10.c
  test2014_11.c
  test2014_12.c
  test2014_13.c
  test2014_14.c
  test2014_15.c
  test2014_16.c
  test2014_17.c
  test2014_18.c
  test2014_19.c
  test2014_20.c
  test2014_21.c
  test2014_22.c
  test2014_23.c
  test2014_24.c
  test2014_25.c
  test2014_26.c
  test2014_27.c
  test2014_28.c
  test2014_29.c
  test2014_30.c
  last.c
)

set(ROSE_FLAGS -w -rose:verbose 0 -std=c11)

foreach(file_to_test ${C11_TESTCODES})
  compile_test(${file_to_test} "C11_tests")
endforeach()

# Makefile check targets.
set(_c11_group_tests)
foreach(file_to_test ${C11_TESTCODES})
  string(REPLACE "/" "_" _test_key "${file_to_test}")
  string(REPLACE "." "_" _test_key "${_test_key}")
  list(APPEND _c11_group_tests "C11_tests_${_test_key}")
endforeach()
add_test(
  NAME nonsmoke_functional_CompileTests_C11_tests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=${_c11_group_tests}
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_C11_tests_check-local PROPERTIES
  DEPENDS "${_c11_group_tests}"
)
