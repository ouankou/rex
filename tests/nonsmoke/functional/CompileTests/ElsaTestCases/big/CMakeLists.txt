set(ROSE_FLAGS -w)
set(_elsa_big_output_dir "${ROSE_TEST_OUTPUT_DIR}")
if(_elsa_big_output_dir STREQUAL "")
  set(_elsa_big_output_dir "${CMAKE_CURRENT_BINARY_DIR}/rose_output")
endif()
file(MAKE_DIRECTORY "${_elsa_big_output_dir}")
file(GLOB _elsa_big_c_sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../ctests/*.c)
set(ELSATEST_BIG_ALL_TESTCODES ${ELSA_TESTCODES})
foreach(_elsa_big_c_source IN LISTS _elsa_big_c_sources)
  string(REGEX REPLACE "^ctests/" "c/" _elsa_big_testcode
    "${_elsa_big_c_source}")
  list(APPEND ELSATEST_BIG_ALL_TESTCODES ${_elsa_big_testcode})
endforeach()
list(SORT ELSATEST_BIG_ALL_TESTCODES)
set(ELSATEST_BIG_GNU89_C_TESTCODES
  c/dC0026.c
  c/dC0027.c
  c/dC0029.c
  c/k0003a.c
  c/k0006.c
  c/k0006a.c
  c/t0006.c
  c/t0011.c
  c/t0012.c
  c/t0014.c
  c/t0015.c
  c/t0016.c
  c/t0021.c
)
set(ELSATEST_BIG_NEGATIVE_TESTCODES
  # These originals are rejected by Clang 22 in the same language mode that the
  # big ELSA sweep uses. Keep them visible as expected negative coverage.
  c/dC0024.c
  c/dC0025.c
  c/t0010.c
  c/t0022.c
  c/t0026.c
  d0064.cc
  d0091.cc
  d0112.cc
  k0001.cc
  k0005.cc
  k0005a.cc
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

function(rex_elsa_big_exact_negative_diagnostic input_file output_variable)
  if(input_file STREQUAL "c/dC0024.c")
    set(_diagnostic "error: cannot combine with previous 'int' declaration specifier")
  elseif(input_file STREQUAL "c/dC0025.c")
    set(_diagnostic "error: second parameter of 'main' (argument array) must be of type 'char **'")
  elseif(input_file STREQUAL "c/t0010.c")
    set(_diagnostic "error: incompatible pointer to integer conversion returning 'void *' from a function with result type 'unsigned long'")
  elseif(input_file STREQUAL "c/t0022.c")
    set(_diagnostic "error: conflicting types for 'main'")
  elseif(input_file STREQUAL "c/t0026.c")
    set(_diagnostic "error: conflicting types for 'foo'")
  elseif(input_file STREQUAL "d0064.cc")
    set(_diagnostic "error: no matching function for call to 'g'")
  elseif(input_file STREQUAL "d0091.cc")
    set(_diagnostic "error: 'operator new' takes type size_t ('unsigned long') as 1st parameter")
  elseif(input_file STREQUAL "d0112.cc")
    set(_diagnostic "error: use 'template' keyword to treat 'rebind' as a dependent template name")
  elseif(input_file STREQUAL "k0001.cc")
    set(_diagnostic "error: reference to non-static member function must be called; did you mean to call it with no arguments?")
  elseif(input_file STREQUAL "k0005.cc")
    set(_diagnostic "error: extra qualification on member 'S1'")
  elseif(input_file STREQUAL "k0005a.cc")
    set(_diagnostic "error: extra qualification on member 'varName'")
  elseif(input_file STREQUAL "k0010.cc")
    set(_diagnostic "error: 'F' declared as an array with a negative size")
  elseif(input_file STREQUAL "k0033.cc")
    set(_diagnostic "error: target exception specification is not superset of source")
  elseif(input_file STREQUAL "k0042.cc")
    set(_diagnostic "error: expected ']'")
  elseif(input_file STREQUAL "k0046a.cc")
    set(_diagnostic "error: cast from pointer to smaller type 'int' loses information")
  elseif(input_file STREQUAL "t0071.cc")
    set(_diagnostic "error: redefinition of 'x'")
  elseif(input_file STREQUAL "t0132.cc")
    set(_diagnostic "error: 'Parent *' and 'Child *' are not pointers to compatible types")
  elseif(input_file STREQUAL "t0243.cc")
    set(_diagnostic "error: 'operator new' takes type size_t ('unsigned long') as 1st parameter")
  elseif(input_file STREQUAL "t0244.cc")
    set(_diagnostic "error: 'func' is a private member of 'A<int>'")
  elseif(input_file STREQUAL "t0253.cc")
    set(_diagnostic "error: call to non-static member function without an object argument")
  elseif(input_file STREQUAL "t0337.cc")
    set(_diagnostic "error: no matching function for call to 'f'")
  elseif(input_file STREQUAL "t0373.cc" OR
         input_file STREQUAL "t0460.cc")
    set(_diagnostic "error: chained comparison 'X < Y > Z' does not behave the same as a mathematical expression")
  elseif(input_file STREQUAL "t0429.cc")
    set(_diagnostic "error: no member named '~B' in namespace 'N'")
  elseif(input_file STREQUAL "t0461.cc")
    set(_diagnostic "error: out-of-line constructor for 'A' cannot have template arguments")
  elseif(input_file STREQUAL "t0464.cc")
    set(_diagnostic "error: chained comparison 'X > Y < Z' does not behave the same as a mathematical expression")
  elseif(input_file STREQUAL "t0466.cc")
    set(_diagnostic "error: no template named 'C'")
  elseif(input_file STREQUAL "t0469.cc")
    set(_diagnostic "error: declaration of reference variable 'c' requires an initializer")
  elseif(input_file STREQUAL "t0470.cc")
    set(_diagnostic "error: use of undeclared identifier 'bar'")
  elseif(input_file STREQUAL "t0482.cc")
    set(_diagnostic "error: 'operator new' takes type size_t ('unsigned long') as 1st parameter")
  elseif(input_file STREQUAL "t0516.cc")
    set(_diagnostic "error: 'zerg' is a private member of 'B<int>::C'")
  elseif(input_file STREQUAL "t0519.cc")
    set(_diagnostic "error: use of undeclared identifier 'x'")
  elseif(input_file STREQUAL "t0539_9.cc")
    set(_diagnostic "error: cannot define or redeclare 'f1' here because namespace 'Q' does not enclose namespace 'D'")
  elseif(input_file STREQUAL "t0546.cc")
    set(_diagnostic "error: 'facet' is a private member of 'std::locale'")
  elseif(input_file STREQUAL "t0558.cc")
    set(_diagnostic "error: use of undeclared identifier 'f'")
  elseif(input_file STREQUAL "t0568.cc")
    set(_diagnostic "error: use of undeclared identifier 'Min'")
  elseif(input_file STREQUAL "t0578.cc")
    set(_diagnostic "error: out-of-line definition of 'f' does not match any declaration in 'A<n>'")
  else()
    set(_diagnostic "")
  endif()
  set(${output_variable} "${_diagnostic}" PARENT_SCOPE)
endfunction()

function(rex_elsa_big_test input_file)
  set(_test_key ${input_file})
  string(REPLACE "/" "_" _test_key "${_test_key}")
  string(REPLACE "." "_" _test_key "${_test_key}")
  set(test_id ELSATEST_BIG_${_test_key})
  set(_rose_flags ${ROSE_FLAGS})
  set(_test_output_dir "${_elsa_big_output_dir}/${_test_key}")

  set(_input_path "${input_file}")
  string(REGEX REPLACE "^c/" "ctests/" _input_path "${_input_path}")
  set(_input_path "${CMAKE_CURRENT_SOURCE_DIR}/../${_input_path}")
  if(NOT EXISTS "${_input_path}")
    message(STATUS "Skipping Elsa big source missing from tree: ${input_file}")
    return()
  endif()
  file(MAKE_DIRECTORY "${_test_output_dir}")

  if(input_file MATCHES "^c/")
    list(FILTER _rose_flags EXCLUDE REGEX "^-std=.*")
    if(input_file IN_LIST ELSATEST_BIG_GNU89_C_TESTCODES)
      list(APPEND _rose_flags -std=gnu89)
    endif()
  else()
    list(FILTER _rose_flags EXCLUDE REGEX "^-std=.*")
    list(APPEND _rose_flags -std=gnu++03)
  endif()
  rex_elsa_big_exact_negative_diagnostic(${input_file} _exact_diagnostic)
  if(NOT _exact_diagnostic STREQUAL "")
    rex_add_clang_frontend_negative_test(
      ${test_id}
      ELSATEST
      ${_test_output_dir}
      frontend-negative.log
      "${_exact_diagnostic}"
      $<TARGET_FILE:${translator}>
      ${_rose_flags} ${ROSE_INCLUDE_FLAGS}
      -I${CMAKE_CURRENT_SOURCE_DIR} -c ${_input_path})
  elseif(input_file IN_LIST ELSATEST_BIG_NEGATIVE_TESTCODES)
    message(FATAL_ERROR
      "No exact ELSA big frontend-negative diagnostic for ${input_file}")
  else()
    list(APPEND _rose_flags ${REX_ELSA_SEMANTIC_PROBE_FLAGS})
    add_test(
      NAME ${test_id}
      WORKING_DIRECTORY ${_test_output_dir}
      COMMAND ${translator}
        ${_rose_flags} ${ROSE_INCLUDE_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR} -c
        ${_input_path})
    set_tests_properties(${test_id} PROPERTIES LABELS ELSATEST)
  endif()
  set_property(TEST ${test_id} APPEND PROPERTY ENVIRONMENT
               "ROSE_TEST_OUTPUT_DIR=${_test_output_dir}")
endfunction()

foreach(file_to_test ${ELSATEST_BIG_ALL_TESTCODES})
  rex_elsa_big_test(${file_to_test})
endforeach()
