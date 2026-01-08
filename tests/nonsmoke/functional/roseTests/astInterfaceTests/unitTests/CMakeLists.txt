include_directories(${ROSE_INCLUDES})

set(_test_support_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/testSupport)
set(_gtest_include
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/testSupport/gtest/include)

add_executable(sageInterfaceTest
  typesAreEqualUnitTests.cpp
  simpleExpressionEvaluatorUnitTests.cpp)
target_include_directories(sageInterfaceTest PRIVATE
  ${_test_support_dir}
  ${_gtest_include})
target_link_libraries(sageInterfaceTest
  roseUnitTestSupport
  gtest_main
  ROSE_DLL
  ${link_with_libraries})

add_executable(sageBuilderTest
  sageBuilderBuildTypes.cpp
  sageBuilderBuildExpressions.cpp
  buildExpressionUnitTests.cpp)
target_include_directories(sageBuilderTest PRIVATE
  ${_test_support_dir}
  ${_gtest_include})
target_link_libraries(sageBuilderTest
  roseUnitTestSupport
  gtest_main
  ROSE_DLL
  ${link_with_libraries})

add_test(
  NAME astInterface_unit_sageInterfaceTest
  COMMAND sageInterfaceTest
)

add_test(
  NAME astInterface_unit_sageBuilderTest
  COMMAND sageBuilderTest
)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_roseTests_astInterfaceTests_unitTests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=astInterface_unit_sageBuilderTest
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astInterfaceTests_unitTests_check-local PROPERTIES
  DEPENDS "astInterface_unit_sageBuilderTest"
)
