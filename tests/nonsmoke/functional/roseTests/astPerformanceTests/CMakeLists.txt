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

add_test(
  NAME testPerformance
  COMMAND testPerformance ${CMAKE_CURRENT_SOURCE_DIR}/tests.conf
)

add_test(
  NAME rosePerformanceTest
  COMMAND rosePerformanceTest "-rose:compilationPerformanceFile ROSE_PERFORMANCE_DATA.csv -c ${CMAKE_CURRENT_SOURCE_DIR}/input.C"
)
