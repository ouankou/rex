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
  c99/d0077.c
  c99/n0001.c
  c99/n0002.c
  d0050elab.cc
  d0082.cc
  d0118.cc
  d0120.cc
  dC0007.c
  g0015.cc
  g0021.cc
  g0024.cc
  g0026.cc
  k0002.cc
  13.3.3.1.4.cc
  13.3.3.2a.cc
  13.3.3.2b.cc
  13.3.3.2d.cc
  13.3.3b.cc
  13.cc
  t0116.cc
  t0120.cc
  t0127.cc
  t0128.cc
  t0129.cc
  t0130.cc
  t0136.cc
  t0137.cc
  t0141.cc
  t0142.cc
  t0143.cc
  t0146.cc
  t0147.cc
  t0148.cc
  t0178.cc
  t0196.cc
  t0230.cc
  t0272.cc
  t0282.cc
  t0297.cc
  t0299.cc
  t0309.cc
  t0311.cc
  t0342.cc
  t0345.cc
  t0347.cc
  t0354.cc
  t0416.cc
  t0423.cc
  t0431.cc
  t0449.cc
  t0452.cc
  t0465.cc
  t0529.cc
  t0533.cc)

function(elsa_not_compilable_test input_file label)
  set(_test_key ${input_file})
  string(REPLACE "/" "_" _test_key "${_test_key}")
  string(REPLACE "." "_" _test_key "${_test_key}")
  set(_test_name_prefix "${label}")
  if(DEFINED REX_COMPILE_TEST_NAME_PREFIX)
    set(_test_name_prefix "${REX_COMPILE_TEST_NAME_PREFIX}")
  endif()
  set(test_id ${_test_name_prefix}_${_test_key})

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

set(REX_COMPILE_TEST_NAME_PREFIX ELSATEST_NOTCOMPILABLE)
foreach(file_to_test ${ELSATEST_NOTCOMPILABLE_ALL_TESTCODES})
  elsa_not_compilable_test(${file_to_test} ELSATEST ${_elsa_notcompilable_roots})
endforeach()
unset(REX_COMPILE_TEST_NAME_PREFIX)

add_subdirectory(gnu)
add_subdirectory(std)
