set(TESTCODES
  12.3.2c.cc 13.1a.cc 13.1d.cc 3.4.3a.cc 7.3.1.2b.cc 7.3.3d.cc 7.3.4b.cc
  7.3.4e.cc 8.2.7b.cc 12.6.2.2a.cc 13.1b.cc 13.1e.cc 13.1g.cc 13.2a.cc
  7.3.3b.cc 7.3.3k.cc 8.2.1.cc 8.3.3.2.cc 12.6.2.2b.cc 13.1c.cc
  13.1f.cc 13.3.1.1.2.cc 7.1.3b.cc 7.3.2.cc 7.3.3c.cc 7.3.3f.cc
  7.3.4d.cc 8.2.7a.cc 7.3.3a.cc)
set(ROSE_FLAGS  -w -std=gnu++03)
set(_elsa_std_output_dir "${ROSE_TEST_OUTPUT_DIR}")
if(_elsa_std_output_dir STREQUAL "")
  set(_elsa_std_output_dir "${CMAKE_CURRENT_BINARY_DIR}/rose_output")
endif()
file(MAKE_DIRECTORY "${_elsa_std_output_dir}")

foreach(file_to_test ${TESTCODES})
  add_test(
    NAME ELSATEST_${file_to_test}
    WORKING_DIRECTORY ${_elsa_std_output_dir}
    COMMAND testTranslator
      ${ROSE_FLAGS} ${ROSE_INCLUDE_FLAGS} -I${CMAKE_CURRENT_SOURCE_DIR} -c
      ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})
  set_tests_properties(
    ELSATEST_${file_to_test}
    PROPERTIES
      LABELS ELSATEST
      ENVIRONMENT "ROSE_TEST_OUTPUT_DIR=${_elsa_std_output_dir}")
endforeach()
