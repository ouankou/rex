set(TESTCODES
  dC0010.c dC0011.c dC0013.c dC0017.c dC0018.c d0124b.c dC0019.c dC0020.c
  dC0021.c dC0022.c dC0023.c dC0024.c dC0025.c dC0026.c dC0027.c dC0028.c
  dC0029.c dC0030.c dC0031.c dC0032.c k0001.c k0002.c k0003.c k0003a.c
  k0004.c k0005.c k0006a.c k0006.c k0007.c k0008.c k0009.c k0010.c
  k0011.c t0001.c t0002.c t0003.c t0005.c t0006.c t0007.c t0008.c
  t0009.c t0010.c t0011.c t0012.c t0014.c t0015.c t0016.c t0017.c
  t0018.c t0019.c t0020.c t0021.c t0023.c t0024.c t0025.c t0026.c)

set(ROSE_DISABLED_TESTCODES
  d0124b.c dC0019.c dC0020.c dC0021.c dC0022.c dC0023.c dC0024.c dC0025.c
  dC0026.c dC0027.c dC0028.c dC0029.c dC0030.c dC0031.c dC0032.c k0001.c
  k0002.c k0003.c k0003a.c k0004.c k0005.c k0006a.c k0006.c k0007.c
  k0008.c k0009.c k0010.c k0011.c t0001.c t0002.c t0003.c t0005.c
  t0006.c t0007.c t0008.c t0009.c t0010.c t0011.c t0012.c t0014.c
  t0015.c t0016.c t0017.c t0018.c t0019.c t0020.c t0021.c t0023.c
  t0024.c t0025.c t0026.c)
set(ROSE_FLAGS -rose:C  -w)

foreach(file_to_test ${TESTCODES})
  compile_test(${file_to_test} ELSATEST)
endforeach()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_ElsaTestCases_ctests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=ELSATEST_dC0010_c\;ELSATEST_dC0011_c\;ELSATEST_dC0013_c\;ELSATEST_dC0017_c\;ELSATEST_dC0018_c
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_ElsaTestCases_ctests_check-local PROPERTIES
  DEPENDS "ELSATEST_dC0010_c;ELSATEST_dC0011_c;ELSATEST_dC0013_c;ELSATEST_dC0017_c;ELSATEST_dC0018_c"
)
