find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; STL_tests require bash")
endif()

set(_stl_eval_script ${CMAKE_CURRENT_SOURCE_DIR}/stl-eval.sh)
set(_stl_eval_cpp11_dir ${CMAKE_CURRENT_BINARY_DIR}/stl_eval_cpp11)
set(_stl_eval_no_cpp11_dir ${CMAKE_CURRENT_BINARY_DIR}/stl_eval_no_cpp11)
set(_stl_eval_no_cleanup_dir ${CMAKE_CURRENT_BINARY_DIR}/stl_eval_no_cleanup)
set(_stl_cpp11_headers_passing
  algorithm bitset complex deque exception fstream functional iomanip ios
  iosfwd iostream istream iterator limits list locale map memory new numeric
  ostream queue set sstream stack stdexcept streambuf string typeinfo utility
  valarray vector)
string(REPLACE ";" " " _stl_cpp11_headers_passing_env
  "${_stl_cpp11_headers_passing}")

file(MAKE_DIRECTORY
  ${_stl_eval_cpp11_dir}
  ${_stl_eval_no_cpp11_dir}
  ${_stl_eval_no_cleanup_dir}
)

set(_stl_eval_cpp11_tests)
set(_stl_eval_no_cleanup_tests)

if(REX_EXTENDED_TEST_TIMEOUTS)
  foreach(_stl_header IN LISTS _stl_cpp11_headers_passing)
    set(_stl_header_cpp11_dir ${_stl_eval_cpp11_dir}/${_stl_header})
    file(MAKE_DIRECTORY ${_stl_header_cpp11_dir})
    add_test(
      NAME stl_eval_cpp11_${_stl_header}
      WORKING_DIRECTORY ${_stl_header_cpp11_dir}
      COMMAND ${CMAKE_COMMAND} -E env
        TOOL1=${CMAKE_CXX_COMPILER}
        TOOL2=$<TARGET_FILE:testTranslator>
        CPP98_STL_TESTS=no
        CPP11_STL_TESTS=yes
        STL_CPP11_HEADERS_PASSING=${_stl_header}
        ${_stl_eval_script}
    )
    list(APPEND _stl_eval_cpp11_tests stl_eval_cpp11_${_stl_header})
  endforeach()
else()
  add_test(
    NAME stl_eval_cpp11
    WORKING_DIRECTORY ${_stl_eval_cpp11_dir}
    COMMAND ${CMAKE_COMMAND} -E env
      TOOL1=${CMAKE_CXX_COMPILER}
      TOOL2=$<TARGET_FILE:testTranslator>
      CPP98_STL_TESTS=no
      CPP11_STL_TESTS=yes
      STL_CPP11_HEADERS_PASSING=${_stl_cpp11_headers_passing_env}
      ${_stl_eval_script}
  )
  list(APPEND _stl_eval_cpp11_tests stl_eval_cpp11)
endif()

add_test(
  NAME stl_eval_no_cpp11
  WORKING_DIRECTORY ${_stl_eval_no_cpp11_dir}
  COMMAND ${CMAKE_COMMAND} -E env
    TOOL1=${CMAKE_CXX_COMPILER}
    TOOL2=$<TARGET_FILE:testTranslator>
    CPP11_STL_TESTS=no
    ${_stl_eval_script}
)

if(REX_EXTENDED_TEST_TIMEOUTS)
  foreach(_stl_header IN LISTS _stl_cpp11_headers_passing)
    set(_stl_header_no_cleanup_dir ${_stl_eval_no_cleanup_dir}/${_stl_header})
    file(MAKE_DIRECTORY ${_stl_header_no_cleanup_dir})
    add_test(
      NAME stl_eval_no_cleanup_${_stl_header}
      WORKING_DIRECTORY ${_stl_header_no_cleanup_dir}
      COMMAND ${CMAKE_COMMAND} -E env
        TOOL1=${CMAKE_CXX_COMPILER}
        TOOL2=$<TARGET_FILE:testTranslator>
        CPP98_STL_TESTS=no
        CPP11_STL_TESTS=yes
        STL_CPP11_HEADERS_PASSING=${_stl_header}
        ${_stl_eval_script} no-cleanup
    )
    list(APPEND _stl_eval_no_cleanup_tests stl_eval_no_cleanup_${_stl_header})
  endforeach()
else()
  add_test(
    NAME stl_eval_no_cleanup
    WORKING_DIRECTORY ${_stl_eval_no_cleanup_dir}
    COMMAND ${CMAKE_COMMAND} -E env
      TOOL1=${CMAKE_CXX_COMPILER}
      TOOL2=$<TARGET_FILE:testTranslator>
      CPP98_STL_TESTS=no
      CPP11_STL_TESTS=yes
      STL_CPP11_HEADERS_PASSING=${_stl_cpp11_headers_passing_env}
      ${_stl_eval_script} no-cleanup
  )
  list(APPEND _stl_eval_no_cleanup_tests stl_eval_no_cleanup)
endif()

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
# are scheduled at the same time. Sanitizer and Valgrind builds run the same
# checks under instrumentation, so they need a larger wall-clock budget and a
# larger CTest processor reservation to avoid overscheduling them in full suites.
if(REX_EXTENDED_TEST_TIMEOUTS)
  set_tests_properties(
    ${_stl_eval_cpp11_tests}
    stl_eval_no_cpp11
    ${_stl_eval_no_cleanup_tests}
    PROPERTIES TIMEOUT 21600 PROCESSORS 16)
elseif(ROSE_USE_SANITIZER OR ROSE_USE_VALGRIND)
  set_tests_properties(${_stl_eval_cpp11_tests} ${_stl_eval_no_cleanup_tests}
    PROPERTIES TIMEOUT 7200 PROCESSORS 8)
else()
  set_tests_properties(${_stl_eval_cpp11_tests} ${_stl_eval_no_cleanup_tests}
    PROPERTIES TIMEOUT 2400)
endif()
