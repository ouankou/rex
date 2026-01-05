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
