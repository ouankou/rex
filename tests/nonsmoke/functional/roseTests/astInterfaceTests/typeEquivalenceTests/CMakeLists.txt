include_directories(${ROSE_INCLUDES})

add_executable(variableTypeTest variableTypeTest.cpp)
target_link_libraries(variableTypeTest ROSE_DLL ${link_with_libraries})

add_executable(functionTypeTest functionTypeTest.cpp)
target_link_libraries(functionTypeTest ROSE_DLL ${link_with_libraries})

add_executable(typecounter countDifferentTypes.cpp)
target_link_libraries(typecounter ROSE_DLL ${link_with_libraries})

set_target_properties(variableTypeTest functionTypeTest typecounter PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  BUILD_WITH_INSTALL_RPATH OFF
  BUILD_RPATH "${CMAKE_BINARY_DIR}/lib"
)

add_test(
  NAME typeEquivalence_variableTypeTest
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/runVariableTypeTest.sh
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
  NAME typeEquivalence_functionTypeTest
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/runFunctionTypeTest.sh
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_test(
  NAME build
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/runVariableTypeTest.sh
    ${CMAKE_CURRENT_BINARY_DIR}
)
set_tests_properties(build PROPERTIES DEPENDS typeEquivalence_variableTypeTest)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_roseTests_astInterfaceTests_typeEquivalenceTests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=typeEquivalence_functionTypeTest\;typeEquivalence_variableTypeTest
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astInterfaceTests_typeEquivalenceTests_check-local PROPERTIES
  DEPENDS "typeEquivalence_functionTypeTest;typeEquivalence_variableTypeTest"
)
add_test(
  NAME nonsmoke_functional_roseTests_astInterfaceTests_typeEquivalenceTests_shell-check
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=typeEquivalence_functionTypeTest\;typeEquivalence_variableTypeTest
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astInterfaceTests_typeEquivalenceTests_shell-check PROPERTIES
  DEPENDS "typeEquivalence_functionTypeTest;typeEquivalence_variableTypeTest"
)
