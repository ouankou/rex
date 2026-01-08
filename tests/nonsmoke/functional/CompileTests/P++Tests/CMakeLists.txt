set(TESTCODES test2001_01.C)
set(ROSE_DISABLED_TESTCODES test2001_01.C)

foreach(file_to_test ${TESTCODES})
  compile_test(${file_to_test} PXXTEST)
endforeach()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_P++Tests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=PXXTEST_test2001_01_C
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_P++Tests_check-local PROPERTIES
  DEPENDS "PXXTEST_test2001_01_C"
)
