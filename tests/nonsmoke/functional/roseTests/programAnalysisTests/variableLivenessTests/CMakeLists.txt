include_directories(${ROSE_INCLUDES})

set(_liveness_output_dir ${CMAKE_CURRENT_BINARY_DIR}/test-output)
file(MAKE_DIRECTORY ${_liveness_output_dir})

add_executable(variableLivenessRunTest runTest.C)
target_link_libraries(variableLivenessRunTest ROSE_DLL ${link_with_libraries})

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_liveness_conf ${CMAKE_CURRENT_SOURCE_DIR}/runTest.conf)

set(_liveness_test_numbers
  1 2 3 4 5 6 7 8 9 10 11
  13 14 15
  18 19 20 21 22 23 24 25
)

foreach(testnum IN LISTS _liveness_test_numbers)
  add_test(
    NAME liveness_runTest_${testnum}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMAND ${_rth}
      TESTNUM=${testnum}
      srcdir=${CMAKE_CURRENT_SOURCE_DIR}
      blddir=${CMAKE_CURRENT_BINARY_DIR}
      ${_liveness_conf}
      variableLivenessRunTest_${testnum}.passed
  )
endforeach()

get_property(_liveness_registered_tests DIRECTORY PROPERTY TESTS)
if(_liveness_registered_tests)
  set_property(TEST ${_liveness_registered_tests} APPEND PROPERTY ENVIRONMENT
               "ROSE_TEST_OUTPUT_DIR=${_liveness_output_dir}")
endif()
