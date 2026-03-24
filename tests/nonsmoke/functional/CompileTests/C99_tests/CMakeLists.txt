set(files_to_test
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

set(ROSE_FLAGS -rose:verbose 0 -rose:C99  -w )

foreach(file_to_test ${files_to_test})
  compile_test(${file_to_test} C99TEST)
endforeach()

