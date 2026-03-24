include_directories(${ROSE_INCLUDES})

add_executable(variableTypeTest variableTypeTest.cpp)
target_link_libraries(variableTypeTest ROSE_DLL ${link_with_libraries})

add_executable(functionTypeTest functionTypeTest.cpp)
target_link_libraries(functionTypeTest ROSE_DLL ${link_with_libraries})

add_executable(typecounter countDifferentTypes.cpp)
target_link_libraries(typecounter ROSE_DLL ${link_with_libraries})

set_target_properties(variableTypeTest functionTypeTest typecounter PROPERTIES
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

