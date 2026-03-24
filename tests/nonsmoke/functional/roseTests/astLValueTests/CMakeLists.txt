include_directories(${ROSE_INCLUDES})

add_executable(testLValues runTest.C)
target_link_libraries(testLValues ROSE_DLL ${link_with_libraries})

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

add_test(
  NAME astLValue_testLValues
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    USE_SUBDIR=yes
    CMD="$<TARGET_FILE:testLValues> ${CMAKE_CURRENT_SOURCE_DIR}/test_astLValues.C"
    ${_exit_status}
    testLValues_1.passed
)

add_test(
  NAME testLValues_1
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND $<TARGET_FILE:testLValues> ${CMAKE_CURRENT_SOURCE_DIR}/test_astLValues.C
)

