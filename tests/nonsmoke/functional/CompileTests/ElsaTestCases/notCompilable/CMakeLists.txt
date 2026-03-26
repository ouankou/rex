set(ROSE_FLAGS -w -std=gnu++03)
file(GLOB ELSATEST_NOTCOMPILABLE_ALL_TESTCODES
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  "*.cc"
  "*.c"
  "c/*.c"
  "c99/*.c")
list(SORT ELSATEST_NOTCOMPILABLE_ALL_TESTCODES)

# Historically in Elsa these lived in notCompilable, but Clang accepts them
# and the translator handles them as ordinary compile tests.
set(ELSATEST_NOTCOMPILABLE_EXPECT_PASS_TESTCODES
  t0178.cc
  t0196.cc)

function(elsa_not_compilable_test input_file label)
  set(_test_key ${input_file})
  string(REPLACE "/" "_" _test_key "${_test_key}")
  string(REPLACE "." "_" _test_key "${_test_key}")
  set(test_id ${label}_${_test_key})

  set(_test_output_dir "${ROSE_TEST_OUTPUT_DIR}")
  if(_test_output_dir STREQUAL "")
    set(_test_output_dir "${CMAKE_CURRENT_BINARY_DIR}/rose_output")
  endif()
  file(MAKE_DIRECTORY "${_test_output_dir}")
  set(_rose_flags ${ROSE_FLAGS})

  set(_source_path "")
  foreach(_root IN LISTS ARGN)
    if(EXISTS "${_root}/${input_file}")
      set(_source_path "${_root}/${input_file}")
      break()
    endif()
  endforeach()
  if(_source_path STREQUAL "")
    message(STATUS
      "Skipping Elsa notCompilable source missing from tree: ${input_file}")
    return()
  endif()

  get_filename_component(_source_dir "${_source_path}" DIRECTORY)
  if(_source_path MATCHES "\\.c$")
    list(FILTER _rose_flags EXCLUDE REGEX "^-std=.*")
    list(APPEND _rose_flags -rose:C)
  endif()

  add_test(
    NAME ${test_id}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${translator}
      ${_rose_flags}
      ${ROSE_INCLUDE_FLAGS}
      -I${CMAKE_CURRENT_SOURCE_DIR}
      -I${_source_dir}
      -c ${_source_path}
  )
  set(_test_will_fail TRUE)
  if(input_file IN_LIST ELSATEST_NOTCOMPILABLE_EXPECT_PASS_TESTCODES)
    set(_test_will_fail FALSE)
  endif()
  set_tests_properties(${test_id} PROPERTIES LABELS "${label}" WILL_FAIL ${_test_will_fail})
  set_property(TEST ${test_id} APPEND PROPERTY ENVIRONMENT
               "ROSE_TEST_OUTPUT_DIR=${_test_output_dir}")
endfunction()

set(_elsa_notcompilable_roots
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)

foreach(file_to_test ${ELSATEST_NOTCOMPILABLE_ALL_TESTCODES})
  elsa_not_compilable_test(${file_to_test} ELSATEST ${_elsa_notcompilable_roots})
endforeach()

add_subdirectory(gnu)
add_subdirectory(std)
