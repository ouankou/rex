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
