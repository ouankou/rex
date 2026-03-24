set(CXX03_TESTCODES
  test2020_03.C
  test2020_01.C
  test2020_02.C
  test2022_super_long_line.C
)

set(ROSE_FLAGS -w -rose:verbose 0 -std=c++03)

foreach(file_to_test ${CXX03_TESTCODES})
  compile_test(${file_to_test} "Cxx03_tests")
endforeach()

