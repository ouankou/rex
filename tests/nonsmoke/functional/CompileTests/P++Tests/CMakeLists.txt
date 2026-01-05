set(TESTCODES test2001_01.C)
set(ROSE_DISABLED_TESTCODES test2001_01.C)

foreach(file_to_test ${TESTCODES})
  compile_test(${file_to_test} PXXTEST)
endforeach()
