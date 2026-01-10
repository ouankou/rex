include_directories(${ROSE_INCLUDES})

set(_test_support_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/testSupport)
set(_gtest_include
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/testSupport/gtest/include)

add_executable(nodeHierarchyTest
  symbolTableUnitTests.cpp
  functionTypeTests.cpp)
target_include_directories(nodeHierarchyTest PRIVATE
  ${_test_support_dir}
  ${_gtest_include})
target_link_libraries(nodeHierarchyTest
  roseUnitTestSupport
  gtest_main
  gtest
  ROSE_DLL
  ${link_with_libraries})

add_test(
  NAME rosetta_nodeHierarchyTest
  COMMAND nodeHierarchyTest
)
set_tests_properties(rosetta_nodeHierarchyTest PROPERTIES DISABLED TRUE)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_roseTests_ROSETTA_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=rosetta_nodeHierarchyTest
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_ROSETTA_check-local PROPERTIES DEPENDS rosetta_nodeHierarchyTest )
