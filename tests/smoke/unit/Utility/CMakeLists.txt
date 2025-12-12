include_directories(${ROSE_INCLUDES} ${Boost_INCLUDE_DIRS})

set(SMOKE_UTILITY_TESTS
    attributeTests
    testMLog
    testStringUtility
    testSourceLocation)

foreach(test_target IN LISTS SMOKE_UTILITY_TESTS)
  add_executable(${test_target} ${test_target}.C)
  target_link_libraries(${test_target} ROSE_DLL ${link_with_libraries})
endforeach()

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

add_test(
  NAME smoke_attributeTests
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    TITLE="AstAttributeMechanism [attributeTests.passed]"
    CMD="$<TARGET_FILE:attributeTests>"
    ${_exit_status}
    attributeTests.passed)

add_test(
  NAME smoke_testMLog
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    TITLE="test mlogging facility (src/util/mlog.h/C) [testMLog.passed]"
    CMD="$<TARGET_FILE:testMLog>"
    ${_exit_status}
    testMLog.passed)

add_test(
  NAME smoke_testStringUtility
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    TITLE="string utilities [testStringUtility.passed]"
    CMD="$<TARGET_FILE:testStringUtility>"
    ${_exit_status}
    testStringUtility.passed)

add_test(
  NAME smoke_testSourceLocation
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    TITLE="test SourceLocation [testSourceLocation.passed]"
    CMD="$<TARGET_FILE:testSourceLocation>"
    ${_exit_status}
    testSourceLocation.passed)

set_tests_properties(smoke_attributeTests smoke_testMLog smoke_testStringUtility smoke_testSourceLocation
  PROPERTIES LABELS "smoke;utility")
