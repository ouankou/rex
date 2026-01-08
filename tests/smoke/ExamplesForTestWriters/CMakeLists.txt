include_directories(${ROSE_INCLUDES})

add_executable(basicLinkTest basicLinkTest.C)
target_link_libraries(basicLinkTest ROSE_DLL ${link_with_libraries})

add_executable(runAlgorithm runAlgorithm.C)
target_link_libraries(runAlgorithm ROSE_DLL ${link_with_libraries})

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)
set(_smoke_run_algorithm_tests)

foreach(alg a b c d)
  add_test(
    NAME smoke_runAlgorithm_${alg}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      TITLE="algorithm ${alg} [runAlgorithm_${alg}.passed]"
      USE_SUBDIR=yes
      CMD="$<TARGET_FILE:runAlgorithm> --algorithm=${alg}"
      ${_exit_status}
      runAlgorithm_${alg}.passed)
  set_tests_properties(smoke_runAlgorithm_${alg} PROPERTIES LABELS "smoke;examples")
  list(APPEND _smoke_run_algorithm_tests "smoke_runAlgorithm_${alg}")
endforeach()
list(JOIN _smoke_run_algorithm_tests "$<SEMICOLON>" _smoke_run_algorithm_tests_joined)

# Makefile check targets.
add_test(
  NAME smoke_ExamplesForTestWriters_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=${_smoke_run_algorithm_tests_joined}
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(smoke_ExamplesForTestWriters_check-local PROPERTIES
  DEPENDS "${_smoke_run_algorithm_tests}"
)
add_test(
  NAME smoke_ExamplesForTestWriters_check-runAlgorithm
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=${_smoke_run_algorithm_tests_joined}
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(smoke_ExamplesForTestWriters_check-runAlgorithm PROPERTIES
  LABELS "disabled"
  DEPENDS "${_smoke_run_algorithm_tests}"
)
if(NOT REX_ENABLE_DISABLED_TESTS)
  set_tests_properties(smoke_ExamplesForTestWriters_check-runAlgorithm PROPERTIES DISABLED TRUE)
endif()
