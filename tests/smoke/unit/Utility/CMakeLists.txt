include_directories(${ROSE_INCLUDES})

set(SMOKE_UTILITY_TESTS
    attributeTests
    testMLog
    testStringUtility
    testSourceLocation
    testBitOps
    testToNumber)
set(_smoke_utility_tests)

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
    "TITLE=AstAttributeMechanism [attributeTests.passed]"
    "CMD=$<TARGET_FILE:attributeTests>"
    ${_exit_status}
    attributeTests.passed)

add_test(
  NAME smoke_testMLog
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    "TITLE=test mlogging facility (src/util/mlog.h/C) [testMLog.passed]"
    "CMD=$<TARGET_FILE:testMLog>"
    ${_exit_status}
    testMLog.passed)

add_test(
  NAME smoke_testStringUtility
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    "TITLE=string utilities [testStringUtility.passed]"
    "CMD=$<TARGET_FILE:testStringUtility>"
    ${_exit_status}
    testStringUtility.passed)

add_test(
  NAME smoke_testSourceLocation
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    "TITLE=test SourceLocation [testSourceLocation.passed]"
    "CMD=$<TARGET_FILE:testSourceLocation>"
    ${_exit_status}
    testSourceLocation.passed)

add_test(
  NAME smoke_testBitOps
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    "TITLE=test bit operations [testBitOps.passed]"
    "CMD=$<TARGET_FILE:testBitOps>"
    ${_exit_status}
    testBitOps.passed)

add_test(
  NAME smoke_testToNumber
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    "TITLE=test number parsing [testToNumber.passed]"
    "CMD=$<TARGET_FILE:testToNumber>"
    ${_exit_status}
    testToNumber.passed)

set_tests_properties(smoke_attributeTests smoke_testStringUtility smoke_testSourceLocation smoke_testBitOps smoke_testToNumber PROPERTIES LABELS "smoke;utility;sanitizer")
set_tests_properties(smoke_testMLog PROPERTIES LABELS "smoke;utility")
foreach(test_target IN LISTS SMOKE_UTILITY_TESTS)
  list(APPEND _smoke_utility_tests "smoke_${test_target}")
endforeach()
list(JOIN _smoke_utility_tests "$<SEMICOLON>" _smoke_utility_tests_joined)

# Makefile check targets.
add_test(
  NAME smoke_unit_Utility_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=${_smoke_utility_tests_joined}
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(smoke_unit_Utility_check-local PROPERTIES DEPENDS "${_smoke_utility_tests}" )
