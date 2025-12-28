set(SAGE_BUILDER_TESTS
  CaseSensitivtyUnitTests
  SgExpressionBuildersUnitTests
  SgTypeBuildersUnitTests
  SgStatementBuildersUnitTests
  SupportNodesUnitTests
  UntypedNodeBuildersUnitTests)

foreach(test_target IN LISTS SAGE_BUILDER_TESTS)
  add_executable(${test_target} ${test_target}.C)
  target_link_libraries(${test_target} unitTestsCommon)
endforeach()

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

foreach(test_target IN LISTS SAGE_BUILDER_TESTS)
  add_test(
    NAME nonsmoke_unittests_${test_target}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="$<TARGET_FILE:${test_target}>"
      ${_exit_status}
      ${test_target}.test)
  set_tests_properties(nonsmoke_unittests_${test_target} PROPERTIES LABELS "nonsmoke;unittests")
endforeach()
