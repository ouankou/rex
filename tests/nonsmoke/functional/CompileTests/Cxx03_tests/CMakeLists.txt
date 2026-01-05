set(CXX03_TESTCODES
  test2020_03.C
  test2020_01.C
  test2020_02.C
)
set(CXX03_DISABLED_TESTCODES
  test2020_01.C
  test2020_02.C
)

set(ROSE_FLAGS -w -rose:verbose 0 -std=c++03)

set(ROSE_DISABLED_TESTCODES ${CXX03_DISABLED_TESTCODES})

foreach(file_to_test ${CXX03_TESTCODES})
  compile_test(${file_to_test} "Cxx03_tests")
endforeach()
