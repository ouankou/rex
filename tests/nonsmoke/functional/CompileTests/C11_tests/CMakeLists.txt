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
  last.c
)

set(ROSE_FLAGS -w -rose:verbose 0 -std=c11)

foreach(file_to_test ${C11_TESTCODES})
  compile_test(${file_to_test} "C11_tests")
endforeach()
