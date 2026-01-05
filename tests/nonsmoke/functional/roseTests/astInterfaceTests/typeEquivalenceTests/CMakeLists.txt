include_directories(${ROSE_INCLUDES})

add_executable(variableTypeTest variableTypeTest.cpp)
target_link_libraries(variableTypeTest ROSE_DLL ${link_with_libraries})

add_executable(functionTypeTest functionTypeTest.cpp)
target_link_libraries(functionTypeTest ROSE_DLL ${link_with_libraries})

add_executable(typecounter countDifferentTypes.cpp)
target_link_libraries(typecounter ROSE_DLL ${link_with_libraries})

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
