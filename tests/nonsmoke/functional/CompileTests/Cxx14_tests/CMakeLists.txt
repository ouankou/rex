set(CXX14_TESTCODES
  test2016_01.C
  test2016_12.C
  test2016_13.C
  test2016_16.C
  test2016_17.C
  test2016_18.C
  test2016_20.C
  test2016_21.C
  test2016_22.C
  test2016_23.C
  test2017_01.C
  test2016_40.C
  test2016_06.C
  test2016_07.C
  test2016_08.C
  test2016_09.C
  test2016_14.C
  test2016_26.C
  test2018_01.C
  test2016_03.C
  test2016_11.C
  test2016_25.C
  test2016_02.C
  test2016_04.C
  test2016_05.C
  test2016_10.C
  test2016_15.C
  test2016_19.C
  test2016_24.C
)
set(CXX14_DISABLED_TESTCODES
  test2016_01.C
  test2016_12.C
  test2016_13.C
  test2016_16.C
  test2016_17.C
  test2016_18.C
  test2016_20.C
  test2016_21.C
  test2016_22.C
  test2016_23.C
  test2017_01.C
  test2016_40.C
  test2016_06.C
)

set(ROSE_FLAGS -w -rose:verbose 0 -std=c++14)

set(ROSE_DISABLED_TESTCODES ${CXX14_DISABLED_TESTCODES})

foreach(file_to_test ${CXX14_TESTCODES})
  compile_test(${file_to_test} "Cxx14_tests")
endforeach()
