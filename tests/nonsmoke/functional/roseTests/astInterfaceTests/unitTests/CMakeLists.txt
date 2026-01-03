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
