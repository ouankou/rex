set(TESTCODES
  dC0010.c dC0011.c dC0013.c dC0017.c dC0018.c d0124b.c dC0019.c dC0020.c
  dC0021.c dC0022.c dC0023.c dC0024.c dC0025.c dC0026.c dC0027.c dC0028.c
  dC0029.c dC0030.c dC0031.c dC0032.c k0001.c k0002.c k0003.c k0003a.c
  k0004.c k0005.c k0006a.c k0006.c k0007.c k0008.c k0009.c k0010.c
  k0011.c t0001.c t0002.c t0003.c t0005.c t0006.c t0007.c t0008.c
  t0009.c t0010.c t0011.c t0012.c t0014.c t0015.c t0016.c t0017.c
  t0018.c t0019.c t0020.c t0021.c t0023.c t0024.c t0025.c t0026.c)
set(ROSE_FLAGS -w)
set(ROSE_GNU89_TESTCODES
  dC0026.c
  dC0027.c
  dC0029.c
  k0003a.c
  k0006.c
  k0006a.c
  t0006.c
  t0011.c
  t0012.c
  t0014.c
  t0015.c
  t0016.c
  t0021.c
)
set(ELSATEST_C_NEGATIVE
  dC0024.c
  dC0025.c
  t0010.c
  t0026.c
)

function(elsa_c_compile_test input_file)
  set(_rose_flags ${ROSE_FLAGS})
  set(ROSE_FLAGS ${_rose_flags})
  if(input_file STREQUAL "dC0024.c")
    set(_expected_diagnostic
      "error: cannot combine with previous 'int' declaration specifier")
  elseif(input_file STREQUAL "dC0025.c")
    set(_expected_diagnostic
      "error: second parameter of 'main' (argument array) must be of type 'char **'")
  elseif(input_file STREQUAL "t0010.c")
    set(_expected_diagnostic
      "error: incompatible pointer to integer conversion returning 'void *' from a function with result type 'unsigned long'")
  elseif(input_file STREQUAL "t0026.c")
    set(_expected_diagnostic "error: conflicting types for 'foo'")
  else()
    set(_expected_diagnostic "")
  endif()
  if(NOT _expected_diagnostic STREQUAL "")
    compile_exact_negative_test(${input_file} ELSATEST
      "${_expected_diagnostic}")
  elseif(input_file IN_LIST ELSATEST_C_NEGATIVE)
    message(FATAL_ERROR
      "No exact ELSA C frontend-negative diagnostic for ${input_file}")
  else()
    list(APPEND _rose_flags ${REX_ELSA_SEMANTIC_PROBE_FLAGS})
    set(ROSE_FLAGS ${_rose_flags})
    compile_test(${input_file} ELSATEST)
  endif()
endfunction()

foreach(file_to_test ${TESTCODES})
  elsa_c_compile_test(${file_to_test})
endforeach()
