include_directories(${ROSE_INCLUDES})

add_library(unitTestsCommon STATIC common.C)
target_include_directories(unitTestsCommon PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${ROSE_INCLUDES})
target_link_libraries(unitTestsCommon ROSE_DLL ${link_with_libraries})

add_subdirectory(Rose)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_UnitTests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=nonsmoke_unittests_CaseSensitivtyUnitTests\;nonsmoke_unittests_SgExpressionBuildersUnitTests\;nonsmoke_unittests_SgStatementBuildersUnitTests\;nonsmoke_unittests_SgTypeBuildersUnitTests\;nonsmoke_unittests_SupportNodesUnitTests\;nonsmoke_unittests_UntypedNodeBuildersUnitTests
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_UnitTests_check-local PROPERTIES DEPENDS "nonsmoke_unittests_CaseSensitivtyUnitTests;nonsmoke_unittests_SgExpressionBuildersUnitTests;nonsmoke_unittests_SgStatementBuildersUnitTests;nonsmoke_unittests_SgTypeBuildersUnitTests;nonsmoke_unittests_SupportNodesUnitTests;nonsmoke_unittests_UntypedNodeBuildersUnitTests" )
