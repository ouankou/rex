set(TESTCODES_1
  d0085.cc d0094.cc g0003.cc g0008.cc g0013.cc g0018.cc g0028.cc g0033.cc
  k0005.cc t0127.cc t0132.cc d0078.cc d0086.cc g0004.cc g0014.cc g0029.cc
  k0001.cc t0124.cc d0081.cc d0089.cc d0092.cc d0093.cc g0001.cc g0002.cc
  g0007.cc g0010.cc g0011.cc g0012.cc g0017.cc g0022.cc g0030.cc g0032.cc
  t0129.cc t0131.cc k0003.cc k0004.cc bugs/gb0003.cc bugs/gb0006.cc
  bugs/gb0012.cc)
set(TESTCODES_2
  asm01.c attr01.c attr02.c c0001.c c0002.c d0083.c dC0001.c dC0003.c
  dC0004.c dC0005.c dC0006.c dC0008.c dC0009.c dC0015.c dC0016.c dC0017.c
  d0095.c t0126.c bugs/gb0001.c bugs/gb0002.c bugs/gb0004.c)

set(ROSE_DISABLED_TESTCODES
  d0094.cc g0003.cc g0008.cc g0013.cc g0018.cc g0028.cc g0033.cc k0005.cc
  t0127.cc t0132.cc d0078.cc d0086.cc g0004.cc g0014.cc g0029.cc k0001.cc
  t0124.cc d0081.cc d0089.cc d0092.cc d0093.cc g0001.cc g0002.cc g0007.cc
  g0010.cc g0011.cc g0012.cc g0017.cc g0022.cc g0030.cc g0032.cc t0129.cc
  t0131.cc k0003.cc k0004.cc bugs/gb0003.cc bugs/gb0006.cc bugs/gb0012.cc
  attr01.c attr02.c c0001.c c0002.c d0083.c dC0001.c dC0003.c dC0004.c
  dC0005.c dC0006.c dC0008.c dC0009.c dC0015.c dC0016.c dC0017.c d0095.c
  t0126.c bugs/gb0001.c bugs/gb0002.c bugs/gb0004.c)

set(ROSE_FLAGS  -w)
foreach(file_to_test ${TESTCODES_1})
  compile_test(${file_to_test} ELSATEST)
endforeach()

list(APPEND ROSE_FLAGS -rose:C)
foreach(file_to_test ${TESTCODES_2})
  compile_test(${file_to_test} ELSATEST)
endforeach()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_ElsaTestCases_gnu_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=ELSATEST_asm01_c\;ELSATEST_d0085_cc
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_ElsaTestCases_gnu_check-local PROPERTIES DEPENDS "ELSATEST_asm01_c;ELSATEST_d0085_cc" )
