add_subdirectory(ctests)
add_subdirectory(big)
add_subdirectory(gnu)
add_subdirectory(kandr)
add_subdirectory(notCompilable)
add_subdirectory(std)

set(ROSE_FLAGS  -w -std=gnu++03)
set(_elsa_output_dir ${CMAKE_CURRENT_BINARY_DIR}/rose_output)
file(MAKE_DIRECTORY ${_elsa_output_dir})
file(GLOB ELSA_TESTCODES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cc")
list(SORT ELSA_TESTCODES)

set(ELSA_TESTCODES_CONTINUE_ON_ERROR
  t0071.cc
  t0253.cc
  t0337.cc
  t0373.cc
  t0460.cc
  t0461.cc
  t0464.cc
  t0469.cc
  t0470.cc
  t0482.cc
)

function(elsa_compile_test input_file)
  set(_rose_flags ${ROSE_FLAGS})
  if(input_file IN_LIST ELSA_TESTCODES_CONTINUE_ON_ERROR)
    list(APPEND _rose_flags -rex:clang:continue-on-error)
  endif()
  set(ROSE_FLAGS ${_rose_flags})
  compile_test(${input_file} ELSATEST)
endfunction()

foreach(file_to_test ${ELSA_TESTCODES})
  elsa_compile_test(${file_to_test})
endforeach()

add_test(
  NAME nonsmoke_functional_CompileTests_ElsaTestCases_t0001.cc
  WORKING_DIRECTORY ${_elsa_output_dir}
  COMMAND testTranslator ${ROSE_FLAGS}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/t0001.cc
)
set_tests_properties(
  nonsmoke_functional_CompileTests_ElsaTestCases_t0001.cc
  PROPERTIES
    LABELS ELSATEST
    DEPENDS ELSATEST_t0001_cc
    ENVIRONMENT "ROSE_TEST_OUTPUT_DIR=${_elsa_output_dir}")
