include_directories(${ROSE_INCLUDES})

add_executable(variableLivenessRunTest runTest.C)
target_link_libraries(variableLivenessRunTest ROSE_DLL ${link_with_libraries})
set_target_properties(variableLivenessRunTest PROPERTIES OUTPUT_NAME runTest)

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
      runTest_${testnum}.passed
  )
endforeach()

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_roseTests_programAnalysisTests_variableLivenessTests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=liveness_runTest_1\;liveness_runTest_10\;liveness_runTest_11\;liveness_runTest_13\;liveness_runTest_14\;liveness_runTest_15\;liveness_runTest_18\;liveness_runTest_19\;liveness_runTest_2\;liveness_runTest_20\;liveness_runTest_21\;liveness_runTest_22\;liveness_runTest_23\;liveness_runTest_24\;liveness_runTest_25\;liveness_runTest_3\;liveness_runTest_4\;liveness_runTest_5\;liveness_runTest_6\;liveness_runTest_7\;liveness_runTest_8\;liveness_runTest_9
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_programAnalysisTests_variableLivenessTests_check-local PROPERTIES DEPENDS "liveness_runTest_1;liveness_runTest_10;liveness_runTest_11;liveness_runTest_13;liveness_runTest_14;liveness_runTest_15;liveness_runTest_18;liveness_runTest_19;liveness_runTest_2;liveness_runTest_20;liveness_runTest_21;liveness_runTest_22;liveness_runTest_23;liveness_runTest_24;liveness_runTest_25;liveness_runTest_3;liveness_runTest_4;liveness_runTest_5;liveness_runTest_6;liveness_runTest_7;liveness_runTest_8;liveness_runTest_9" )
