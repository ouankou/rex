include_directories(${ROSE_INCLUDES})

add_executable(defUseRunTest runTest.C)
target_link_libraries(defUseRunTest ROSE_DLL ${link_with_libraries})
set_target_properties(defUseRunTest PROPERTIES OUTPUT_NAME runTest)

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_defuse_conf ${CMAKE_CURRENT_SOURCE_DIR}/runTest.conf)

set(_defuse_test_numbers
  1 2 3 4 5 6 7 8 9 10
  11 12 13 14 15 16 17 18 19 20
  21 22 23 24 25
)

foreach(testnum IN LISTS _defuse_test_numbers)
  add_test(
    NAME defuse_runTest_${testnum}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMAND ${_rth}
      TESTNUM=${testnum}
      srcdir=${CMAKE_CURRENT_SOURCE_DIR}
      blddir=${CMAKE_CURRENT_BINARY_DIR}
      ${_defuse_conf}
      runTest_${testnum}.passed
  )
endforeach()
