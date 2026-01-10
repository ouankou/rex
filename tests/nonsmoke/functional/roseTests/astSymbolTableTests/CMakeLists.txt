#-------------------------------------------------------------------------------
add_executable(testSymbolTable testSymbolTable.C)
target_link_libraries(testSymbolTable
  ROSE_DLL ${link_with_libraries})

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

add_test(
  NAME astSymbolTable_test1
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    "TITLE=testSymbolTable input.C [test1.passed]"
    "USE_SUBDIR=yes"
    "CMD=$<TARGET_FILE:testSymbolTable> -c ${CMAKE_CURRENT_SOURCE_DIR}/input.C"
    ${_exit_status}
    test1.passed
)

add_test(
  NAME testSymbolTable_test1
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testSymbolTable>
    -c ${CMAKE_CURRENT_SOURCE_DIR}/input.C
)


# Makefile check targets.
add_test(
  NAME nonsmoke_functional_roseTests_astSymbolTableTests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=astSymbolTable_test1
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astSymbolTableTests_check-local PROPERTIES DEPENDS astSymbolTable_test1 )
