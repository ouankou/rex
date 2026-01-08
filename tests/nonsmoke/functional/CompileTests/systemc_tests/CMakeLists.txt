include_directories(${ROSE_INCLUDES})

add_executable(member_function member_function.cpp)
target_link_libraries(member_function ROSE_DLL ${link_with_libraries})

add_test(
  NAME systemc_member_function
  COMMAND member_function -c
    -I${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/input_member_function.cpp
)

add_test(
  NAME systemc_input_hello_world
  COMMAND testTranslator -w
    -I${CMAKE_BINARY_DIR}
    -I${CMAKE_CURRENT_SOURCE_DIR}/include
    -c ${CMAKE_CURRENT_SOURCE_DIR}/input_hello_world.C
)
set_tests_properties(systemc_input_hello_world PROPERTIES DISABLED TRUE)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_systemc_tests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=systemc_member_function
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_systemc_tests_check-local PROPERTIES
  DEPENDS "systemc_member_function"
)
