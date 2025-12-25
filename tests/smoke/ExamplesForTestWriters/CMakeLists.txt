include_directories(${ROSE_INCLUDES})

add_executable(basicLinkTest basicLinkTest.C)
target_link_libraries(basicLinkTest ROSE_DLL ${link_with_libraries})

add_executable(runAlgorithm runAlgorithm.C)
target_link_libraries(runAlgorithm ROSE_DLL ${link_with_libraries})

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

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
endforeach()
