find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; STL_tests require bash")
endif()

set(_stl_eval_script ${CMAKE_CURRENT_SOURCE_DIR}/stl-eval.sh)

add_test(
  NAME stl_eval_cpp11
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    TOOL1=${CMAKE_CXX_COMPILER}
    TOOL2=$<TARGET_FILE:testTranslator>
    CPP11_STL_TESTS=yes
    ${_stl_eval_script}
)

set(_stl_disable_cpp11 TRUE)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "4.8")
  set(_stl_disable_cpp11 FALSE)
endif()
set_tests_properties(stl_eval_cpp11 PROPERTIES DISABLED ${_stl_disable_cpp11})

add_test(
  NAME stl_eval_no_cpp11
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    TOOL1=${CMAKE_CXX_COMPILER}
    TOOL2=$<TARGET_FILE:testTranslator>
    CPP11_STL_TESTS=no
    ${_stl_eval_script}
)
set_tests_properties(stl_eval_no_cpp11 PROPERTIES DISABLED TRUE)

add_test(
  NAME stl_eval_no_cleanup
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    TOOL1=${CMAKE_CXX_COMPILER}
    TOOL2=$<TARGET_FILE:testTranslator>
    ${_stl_eval_script} no-cleanup
)
set_tests_properties(stl_eval_no_cleanup PROPERTIES DISABLED TRUE)

add_test(
  NAME stl_eval_only_cleanup
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    TOOL1=${CMAKE_CXX_COMPILER}
    TOOL2=$<TARGET_FILE:testTranslator>
    ${_stl_eval_script} only-cleanup
)
set_tests_properties(stl_eval_only_cleanup PROPERTIES DISABLED TRUE)
