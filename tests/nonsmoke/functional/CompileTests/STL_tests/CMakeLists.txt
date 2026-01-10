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

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_STL_tests_check-no-cleanup
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=stl_eval_cpp11
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_STL_tests_check-no-cleanup PROPERTIES LABELS disabled DEPENDS stl_eval_cpp11 )
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(nonsmoke_functional_CompileTests_STL_tests_check-no-cleanup PROPERTIES DISABLED TRUE)
endif()
add_test(
  NAME nonsmoke_functional_CompileTests_STL_tests_conditional-check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=stl_eval_cpp11
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_STL_tests_conditional-check-local PROPERTIES DEPENDS stl_eval_cpp11 )
