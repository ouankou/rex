set(ROSE_FLAGS -rose:C -w -std=gnu89)

set(TESTCODES t0001.c t0002.c t0003.c t0004.c)

foreach(file_to_test ${TESTCODES})
  compile_test(${file_to_test} ELSATEST)
endforeach()
