find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; STL_tests require bash")
endif()

set(_stl_eval_script ${CMAKE_CURRENT_SOURCE_DIR}/stl-eval.sh)
set(_stl_eval_cpp11_dir ${CMAKE_CURRENT_BINARY_DIR}/stl_eval_cpp11)
set(_stl_eval_no_cpp11_dir ${CMAKE_CURRENT_BINARY_DIR}/stl_eval_no_cpp11)
set(_stl_eval_no_cleanup_dir ${CMAKE_CURRENT_BINARY_DIR}/stl_eval_no_cleanup)
file(MAKE_DIRECTORY
  ${_stl_eval_cpp11_dir}
  ${_stl_eval_no_cpp11_dir}
  ${_stl_eval_no_cleanup_dir}
)

add_test(
  NAME stl_eval_cpp11
  WORKING_DIRECTORY ${_stl_eval_cpp11_dir}
  COMMAND ${CMAKE_COMMAND} -E env
    TOOL1=${CMAKE_CXX_COMPILER}
    TOOL2=$<TARGET_FILE:testTranslator>
    CPP98_STL_TESTS=no
    CPP11_STL_TESTS=yes
    ${_stl_eval_script}
)

add_test(
  NAME stl_eval_no_cpp11
  WORKING_DIRECTORY ${_stl_eval_no_cpp11_dir}
  COMMAND ${CMAKE_COMMAND} -E env
    TOOL1=${CMAKE_CXX_COMPILER}
    TOOL2=$<TARGET_FILE:testTranslator>
    CPP11_STL_TESTS=no
    ${_stl_eval_script}
)

add_test(
  NAME stl_eval_no_cleanup
  WORKING_DIRECTORY ${_stl_eval_no_cleanup_dir}
  COMMAND ${CMAKE_COMMAND} -E env
    TOOL1=${CMAKE_CXX_COMPILER}
    TOOL2=$<TARGET_FILE:testTranslator>
    CPP98_STL_TESTS=no
    CPP11_STL_TESTS=yes
    ${_stl_eval_script} no-cleanup
)

add_test(
  NAME stl_eval_only_cleanup
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E env
    TOOL1=${CMAKE_CXX_COMPILER}
    TOOL2=$<TARGET_FILE:testTranslator>
    ${_stl_eval_script} only-cleanup
)

# LLVM 22/libstdc++ C++11 aggregate header checks still complete normally, but
# they can exceed CTest's default 1500 second timeout when other long STL modes
# are scheduled at the same time.
set_tests_properties(stl_eval_cpp11 stl_eval_no_cleanup PROPERTIES TIMEOUT 2400)
