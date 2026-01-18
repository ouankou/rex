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

set(_type_equiv_tests "")

file(GLOB _type_equiv_var_inputs
  "${CMAKE_CURRENT_SOURCE_DIR}/inputFiles/types_*.cpp")
list(SORT _type_equiv_var_inputs)
foreach(_input IN LISTS _type_equiv_var_inputs)
  get_filename_component(_stem "${_input}" NAME_WE)
  set(_test_name "typeEquivalence_variableTypeTest_${_stem}")
  add_test(
    NAME ${_test_name}
    COMMAND $<TARGET_FILE:variableTypeTest> "${_input}"
  )
  list(APPEND _type_equiv_tests "${_test_name}")
endforeach()

file(GLOB _type_equiv_func_inputs
  "${CMAKE_CURRENT_SOURCE_DIR}/inputFiles/functions_*.cpp")
list(SORT _type_equiv_func_inputs)
foreach(_input IN LISTS _type_equiv_func_inputs)
  get_filename_component(_stem "${_input}" NAME_WE)
  set(_test_name "typeEquivalence_functionTypeTest_${_stem}")
  add_test(
    NAME ${_test_name}
    COMMAND $<TARGET_FILE:functionTypeTest> "${_input}"
  )
  list(APPEND _type_equiv_tests "${_test_name}")
endforeach()

string(REPLACE ";" "\\;" _type_equiv_tests_arg "${_type_equiv_tests}")


# Makefile check targets.
add_test(
  NAME nonsmoke_functional_roseTests_astInterfaceTests_typeEquivalenceTests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=${_type_equiv_tests_arg}
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astInterfaceTests_typeEquivalenceTests_check-local PROPERTIES DEPENDS "${_type_equiv_tests}" )
