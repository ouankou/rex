################################################################################
# testPerformance -- tests how fast the "new" operator works
################################################################################
add_executable(testPerformance testPerformance.C)
target_link_libraries(testPerformance ROSE_DLL ${link_with_libraries})

################################################################################
# rosePerformanceTest
################################################################################
add_executable(rosePerformanceTest rosePerformanceTest.C)
target_link_libraries(rosePerformanceTest ROSE_DLL ${link_with_libraries})

install(TARGETS testPerformance rosePerformanceTest DESTINATION bin)

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_tests_conf ${CMAKE_CURRENT_SOURCE_DIR}/tests.conf)

add_test(
  NAME testPerformance
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    EXE=$<TARGET_FILE:testPerformance>
    ${_tests_conf}
    testPerformance.passed
)

add_test(
  NAME rosePerformanceTest
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${_rth}
    EXE=$<TARGET_FILE:rosePerformanceTest>
    ARGS="-rose:compilationPerformanceFile ROSE_PERFORMANCE_DATA.csv -c ${CMAKE_CURRENT_SOURCE_DIR}/input.C"
    ${_tests_conf}
    rosePerformanceTest.passed
)

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_roseTests_astPerformanceTests_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=rosePerformanceTest\;testPerformance
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_roseTests_astPerformanceTests_check-local PROPERTIES
  DEPENDS "rosePerformanceTest;testPerformance"
)
