set(files_to_test
  test2005_186.c
  test2006_127.c
  test2006_143.c
  test2008_01.c
  test2012_20.c
  test2012_23.c
  test2012_25.c
  test2012_58.c
  test2012_100.c
  test2012_104.c
  test2012_105.c
  test2013_01.c
  test2013_02.c
  test2013_03.c
  test2013_04.c
  test2014_01.c
  complex_01.c
  complex_02.c
  complex_03.c
  complex_04.c
  bool.c
  constants.c
  test2012_106.c
)

set(ROSE_DISABLED_TESTCODES
  complex_02.c
  complex_04.c
  test2012_106.c
)

set(ROSE_FLAGS -rose:verbose 0 -rose:C99  -w )

foreach(file_to_test ${files_to_test})
  compile_test(${file_to_test} C99TEST)
endforeach()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_C99_tests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=C99TEST_bool_c\;C99TEST_complex_01_c\;C99TEST_complex_03_c\;C99TEST_constants_c\;C99TEST_test2005_186_c\;C99TEST_test2006_127_c\;C99TEST_test2006_143_c\;C99TEST_test2008_01_c\;C99TEST_test2012_100_c\;C99TEST_test2012_104_c\;C99TEST_test2012_105_c\;C99TEST_test2012_20_c\;C99TEST_test2012_23_c\;C99TEST_test2012_25_c\;C99TEST_test2012_58_c\;C99TEST_test2013_01_c\;C99TEST_test2013_02_c\;C99TEST_test2013_03_c\;C99TEST_test2013_04_c\;C99TEST_test2014_01_c
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_C99_tests_check-local PROPERTIES
  DEPENDS "C99TEST_bool_c;C99TEST_complex_01_c;C99TEST_complex_03_c;C99TEST_constants_c;C99TEST_test2005_186_c;C99TEST_test2006_127_c;C99TEST_test2006_143_c;C99TEST_test2008_01_c;C99TEST_test2012_100_c;C99TEST_test2012_104_c;C99TEST_test2012_105_c;C99TEST_test2012_20_c;C99TEST_test2012_23_c;C99TEST_test2012_25_c;C99TEST_test2012_58_c;C99TEST_test2013_01_c;C99TEST_test2013_02_c;C99TEST_test2013_03_c;C99TEST_test2013_04_c;C99TEST_test2014_01_c"
)
