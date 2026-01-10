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

add_test(
  NAME CaseSensitivtyUnitTests.test
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND $<TARGET_FILE:CaseSensitivtyUnitTests>
)
set_tests_properties(CaseSensitivtyUnitTests.test PROPERTIES LABELS "nonsmoke;unittests")

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_UnitTests_Rose_SageBuilder_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=nonsmoke_unittests_CaseSensitivtyUnitTests\;nonsmoke_unittests_SgExpressionBuildersUnitTests\;nonsmoke_unittests_SgStatementBuildersUnitTests\;nonsmoke_unittests_SgTypeBuildersUnitTests\;nonsmoke_unittests_SupportNodesUnitTests\;nonsmoke_unittests_UntypedNodeBuildersUnitTests
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_UnitTests_Rose_SageBuilder_check-local PROPERTIES DEPENDS "nonsmoke_unittests_CaseSensitivtyUnitTests;nonsmoke_unittests_SgExpressionBuildersUnitTests;nonsmoke_unittests_SgStatementBuildersUnitTests;nonsmoke_unittests_SgTypeBuildersUnitTests;nonsmoke_unittests_SupportNodesUnitTests;nonsmoke_unittests_UntypedNodeBuildersUnitTests" )
