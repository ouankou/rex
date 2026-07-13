set(C11_TESTCODES
  test2014_01.c
  test2014_02.c
  test2014_04.c
  test2014_05.c
  test2014_03.c
  test2014_06.c
  test2014_07.c
  test2014_08.c
  test2014_09.c
  test2014_10.c
  test2014_11.c
  test2014_12.c
  test2014_13.c
  test2014_14.c
  test2014_15.c
  test2014_16.c
  test2014_17.c
  test2014_18.c
  test2014_19.c
  test2014_20.c
  test2014_21.c
  test2014_22.c
  test2014_23.c
  test2014_24.c
  test2014_25.c
  test2014_26.c
  test2014_27.c
  test2014_28.c
  test2014_29.c
  test2014_30.c
  test2014_31.c
  last.c
)

set(C11_NEGATIVE_TESTCODES
  test2014_02.c
  test2014_17.c
  test2014_19.c
)
set(C11_POSITIVE_TESTCODES ${C11_TESTCODES})
list(REMOVE_ITEM C11_POSITIVE_TESTCODES ${C11_NEGATIVE_TESTCODES})

set(ROSE_FLAGS -w -rose:verbose 0 -std=c11)

rex_register_compile_source_set(REX_C11_TESTS_SOURCES ${C11_TESTCODES})
rex_register_compile_source_set(REX_C11_TESTS_NEGATIVE_SOURCES
  ${C11_NEGATIVE_TESTCODES})

foreach(file_to_test ${C11_POSITIVE_TESTCODES})
  compile_test(${file_to_test} "C11_tests")
endforeach()

foreach(file_to_test ${C11_NEGATIVE_TESTCODES})
  if(file_to_test STREQUAL "test2014_02.c")
    set(_expected_diagnostic "error: expected ';' after struct")
  elseif(file_to_test STREQUAL "test2014_17.c")
    set(_expected_diagnostic
      "error: call to undeclared library function 'aligned_alloc' with type 'void *(__size_t, __size_t)' (aka 'void *(unsigned long, unsigned long)'); ISO C99 and later do not support implicit function declarations")
  elseif(file_to_test STREQUAL "test2014_19.c")
    set(_expected_diagnostic
      "error: 'atomic' attribute cannot be applied to a declaration")
  else()
    message(FATAL_ERROR
      "No exact C11 frontend-negative diagnostic for ${file_to_test}")
  endif()
  compile_exact_negative_test(${file_to_test} "C11_tests"
    "${_expected_diagnostic}")
endforeach()

rex_verify_compile_test_source_coverage(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  PROPERTIES REX_C11_TESTS_SOURCES
)
