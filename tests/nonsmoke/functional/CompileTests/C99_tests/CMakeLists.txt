set(files_to_test
  test2006_13.c
  test2005_186.c
  test2006_127.c
  test2006_143.c
  test2008_01.c
  test2012_20.c
  test2012_23.c
  test2012_25.c
  test2012_58.c
  test2012_100.c
  test2012_104.c
  test2012_105.c
  test2013_01.c
  test2013_02.c
  test2013_03.c
  test2013_04.c
  test2014_01.c
  complex_01.c
  complex_02.c
  complex_03.c
  complex_04.c
  bool.c
  constants.c
  test2012_106.c
)

set(negative_files
  complex_01.c
  complex_02.c
)
list(REMOVE_ITEM files_to_test ${negative_files})

set(ROSE_FLAGS -rose:verbose 0 -rose:C99  -w )

rex_register_compile_source_set(REX_C99_TESTS_SOURCES
  ${files_to_test}
  ${negative_files}
)

foreach(file_to_test ${files_to_test})
  compile_test(${file_to_test} C99TEST)
endforeach()

foreach(file_to_test ${negative_files})
  compile_negative_test(${file_to_test} C99TEST)
endforeach()

rex_verify_compile_test_source_coverage(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  PROPERTIES REX_C99_TESTS_SOURCES
)
