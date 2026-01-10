set(CXX03_TESTCODES
  test2020_03.C
  test2020_01.C
  test2020_02.C
  test2022_super_long_line.C
)
set(CXX03_DISABLED_TESTCODES
  test2020_01.C
  test2020_02.C
)

set(ROSE_FLAGS -w -rose:verbose 0 -std=c++03)

set(ROSE_DISABLED_TESTCODES ${CXX03_DISABLED_TESTCODES})

foreach(file_to_test ${CXX03_TESTCODES})
  compile_test(${file_to_test} "Cxx03_tests")
endforeach()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_Cxx03_tests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=Cxx03_tests_test2020_03_C\;Cxx03_tests_test2022_super_long_line_C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_Cxx03_tests_check-local PROPERTIES DEPENDS "Cxx03_tests_test2020_03_C;Cxx03_tests_test2022_super_long_line_C" )
