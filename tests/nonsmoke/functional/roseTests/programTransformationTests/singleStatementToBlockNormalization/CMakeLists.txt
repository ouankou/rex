include_directories(${ROSE_INCLUDES})
include_directories(
  ${CMAKE_SOURCE_DIR}/src/midend/programTransformation/singleStatementToBlockNormalization)

add_executable(singleStatementToBlockNormalization
  SingleStatementToBlockNormalization.C)
target_link_libraries(singleStatementToBlockNormalization
  ROSE_DLL ${link_with_libraries})

include(${CMAKE_CURRENT_SOURCE_DIR}/singleStatementToBlockNormalization_tests.cmake)

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

set(_single_stmt_tests_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)
set(_single_stmt_switches
  -w -rose:verbose 0
  -I${_single_stmt_tests_dir})
list(JOIN _single_stmt_switches " " _single_stmt_switches_str)

set(_single_stmt_output_dir ${CMAKE_CURRENT_BINARY_DIR}/rose_test_output)
file(MAKE_DIRECTORY ${_single_stmt_output_dir})

set(_single_stmt_disabled
)

set(_single_stmt_test_targets)
foreach(specimen IN LISTS SINGLE_STMT_BLOCK_TEST_SOURCES)
  set(_single_stmt_target singleStatementToBlockNormalization_${specimen}.passed)
  add_test(
    NAME singleStatementToBlockNormalization_${specimen}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="$<TARGET_FILE:singleStatementToBlockNormalization> ${_single_stmt_switches_str} -c ${_single_stmt_tests_dir}/${specimen}"
      ${_exit_status}
      ${_single_stmt_target}
  )
  list(APPEND _single_stmt_test_targets
    singleStatementToBlockNormalization_${specimen})
  set_tests_properties(singleStatementToBlockNormalization_${specimen} PROPERTIES
    ENVIRONMENT "ROSE_TEST_OUTPUT_DIR=${_single_stmt_output_dir}")
  if(specimen IN_LIST _single_stmt_disabled)
    set_tests_properties(singleStatementToBlockNormalization_${specimen} PROPERTIES DISABLED TRUE)
  endif()
endforeach()

# Makefile check targets.
list(JOIN _single_stmt_test_targets "\;" _single_stmt_group_tests)
add_test(
  NAME nonsmoke_functional_roseTests_programTransformationTests_singleStatementToBlockNormalization_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=${_single_stmt_group_tests}
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(
  nonsmoke_functional_roseTests_programTransformationTests_singleStatementToBlockNormalization_check-local
  PROPERTIES DEPENDS "${_single_stmt_test_targets}"
)
