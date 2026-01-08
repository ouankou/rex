set(TESTCODES
  12.3.2c.cc 13.1a.cc 13.1d.cc 3.4.3a.cc 7.3.1.2b.cc 7.3.3d.cc 7.3.4b.cc
  7.3.4e.cc 8.2.7b.cc 12.6.2.2a.cc 13.1b.cc 13.1e.cc 13.1g.cc 13.2a.cc
  7.3.3b.cc 7.3.3k.cc 8.2.1.cc 8.3.3.2.cc 12.6.2.2b.cc 13.1c.cc
  13.1f.cc 13.3.1.1.2.cc 7.1.3b.cc 7.3.2.cc 7.3.3c.cc 7.3.3f.cc
  7.3.4d.cc 8.2.7a.cc 7.3.3a.cc)
set(ROSE_FLAGS  -w)
set(ROSE_DISABLED_TESTCODES
  7.3.4e.cc 8.2.7b.cc 12.6.2.2a.cc 13.1b.cc 13.1e.cc 13.1g.cc 13.2a.cc
  7.3.3b.cc 7.3.3k.cc 8.2.1.cc 8.3.3.2.cc 12.6.2.2b.cc 13.1c.cc
  13.1f.cc 13.3.1.1.2.cc 7.1.3b.cc 7.3.2.cc 7.3.3c.cc 7.3.3f.cc
  7.3.4d.cc 8.2.7a.cc 7.3.3a.cc)

foreach(file_to_test ${TESTCODES})
  add_test(
    NAME ELSATEST_${file_to_test}
    COMMAND testTranslator
      ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR} -c
      ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})
  set_tests_properties(ELSATEST_${file_to_test} PROPERTIES LABELS ELSATEST)
  if(file_to_test IN_LIST ROSE_DISABLED_TESTCODES)
    set_tests_properties(ELSATEST_${file_to_test} PROPERTIES DISABLED TRUE)
  endif()
endforeach()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_ElsaTestCases_std_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=ELSATEST_12.3.2c.cc\;ELSATEST_13.1a.cc\;ELSATEST_13.1d.cc\;ELSATEST_3.4.3a.cc\;ELSATEST_7.3.1.2b.cc\;ELSATEST_7.3.3d.cc\;ELSATEST_7.3.4b.cc
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_ElsaTestCases_std_check-local PROPERTIES
  DEPENDS "ELSATEST_12.3.2c.cc;ELSATEST_13.1a.cc;ELSATEST_13.1d.cc;ELSATEST_3.4.3a.cc;ELSATEST_7.3.1.2b.cc;ELSATEST_7.3.3d.cc;ELSATEST_7.3.4b.cc"
)
