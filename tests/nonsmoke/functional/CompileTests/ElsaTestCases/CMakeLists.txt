set(REX_ELSA_SEMANTIC_PROBE_FLAGS
  -include
  ${CMAKE_CURRENT_SOURCE_DIR}/rex_elsa_semantic_probe_compat.h)

file(GLOB ELSA_TESTCODES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cc")
list(SORT ELSA_TESTCODES)

add_subdirectory(ctests)
add_subdirectory(big)
add_subdirectory(gnu)
add_subdirectory(kandr)
add_subdirectory(notCompilable)
add_subdirectory(std)

rex_assert_unique_ctest_names_across_directories(
  "ELSA ctests and K&R suites"
  "${CMAKE_CURRENT_BINARY_DIR}/ctests"
  "${CMAKE_CURRENT_BINARY_DIR}/kandr")

set(ROSE_FLAGS  -w -std=gnu++03)
set(_elsa_output_dir ${CMAKE_CURRENT_BINARY_DIR}/rose_output)
file(MAKE_DIRECTORY ${_elsa_output_dir})

set(ELSA_TESTCODES_NEGATIVE
  # Clang 22 rejects these original ELSA specimens in GNU++03 mode. Keep them
  # registered as hard negative frontend coverage instead of continuing past
  # diagnostics and compiling a partial AST.
  d0064.cc
  d0091.cc
  d0112.cc
  k0005.cc
  k0005a.cc
  k0001.cc
  k0010.cc
  k0033.cc
  k0042.cc
  k0046a.cc
  t0071.cc
  t0132.cc
  t0243.cc
  t0244.cc
  t0253.cc
  t0337.cc
  t0373.cc
  t0429.cc
  t0460.cc
  t0461.cc
  t0464.cc
  t0466.cc
  t0469.cc
  t0470.cc
  t0482.cc
  t0516.cc
  t0519.cc
  t0539_9.cc
  t0546.cc
  t0558.cc
  t0568.cc
  t0578.cc
)

function(elsa_exact_negative_diagnostic input_file output_variable)
  if(input_file STREQUAL "t0071.cc")
    set(_diagnostic "error: redefinition of 'x'")
  elseif(input_file STREQUAL "t0253.cc")
    set(_diagnostic "error: call to non-static member function without an object argument")
  elseif(input_file STREQUAL "t0337.cc")
    set(_diagnostic "error: no matching function for call to 'f'")
  elseif(input_file STREQUAL "t0373.cc" OR
         input_file STREQUAL "t0460.cc")
    set(_diagnostic "error: chained comparison 'X < Y > Z' does not behave the same as a mathematical expression")
  elseif(input_file STREQUAL "t0461.cc")
    set(_diagnostic "error: out-of-line constructor for 'A' cannot have template arguments")
  elseif(input_file STREQUAL "t0464.cc")
    set(_diagnostic "error: chained comparison 'X > Y < Z' does not behave the same as a mathematical expression")
  elseif(input_file STREQUAL "t0469.cc")
    set(_diagnostic "error: declaration of reference variable 'c' requires an initializer")
  elseif(input_file STREQUAL "t0470.cc")
    set(_diagnostic "error: use of undeclared identifier 'bar'")
  elseif(input_file STREQUAL "t0482.cc")
    set(_diagnostic "error: 'operator new' takes type size_t ('unsigned long') as 1st parameter")
  else()
    set(_diagnostic "")
  endif()
  set(${output_variable} "${_diagnostic}" PARENT_SCOPE)
endfunction()

function(elsa_compile_test input_file)
  set(_rose_flags ${ROSE_FLAGS})
  set(ROSE_FLAGS ${_rose_flags})
  if(input_file STREQUAL "k0001.cc")
    set(REX_COMPILE_TEST_NAME_PREFIX ELSATEST_ROOT)
  endif()
  elsa_exact_negative_diagnostic(${input_file} _exact_diagnostic)
  if(NOT _exact_diagnostic STREQUAL "")
    compile_exact_negative_test(${input_file} ELSATEST
      "${_exact_diagnostic}")
  elseif(input_file IN_LIST ELSA_TESTCODES_NEGATIVE)
    compile_negative_test(${input_file} ELSATEST)
  else()
    list(APPEND _rose_flags ${REX_ELSA_SEMANTIC_PROBE_FLAGS})
    set(ROSE_FLAGS ${_rose_flags})
    compile_test(${input_file} ELSATEST)
  endif()
endfunction()

foreach(file_to_test ${ELSA_TESTCODES})
  elsa_compile_test(${file_to_test})
endforeach()

add_test(
  NAME nonsmoke_functional_CompileTests_ElsaTestCases_t0001.cc
  WORKING_DIRECTORY ${_elsa_output_dir}
  COMMAND testTranslator ${ROSE_FLAGS}
    ${REX_ELSA_SEMANTIC_PROBE_FLAGS}
    -c ${CMAKE_CURRENT_SOURCE_DIR}/t0001.cc
)
set_tests_properties(
  nonsmoke_functional_CompileTests_ElsaTestCases_t0001.cc
  PROPERTIES
    LABELS ELSATEST
    DEPENDS ELSATEST_t0001_cc
    ENVIRONMENT "ROSE_TEST_OUTPUT_DIR=${_elsa_output_dir}")
