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
  -I${_single_stmt_tests_dir}
  -I${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/A++Code)

set(_single_stmt_disabled
  test2013_242.C
)

foreach(specimen IN LISTS SINGLE_STMT_BLOCK_TEST_SOURCES)
  set(_single_stmt_target singleStatementToBlockNormalization_${specimen}.passed)
  add_test(
    NAME singleStatementToBlockNormalization_${specimen}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="./singleStatementToBlockNormalization ${_single_stmt_switches} -c ${_single_stmt_tests_dir}/${specimen}"
      ${_exit_status}
      ${_single_stmt_target}
  )
  if(specimen IN_LIST _single_stmt_disabled)
    set_tests_properties(singleStatementToBlockNormalization_${specimen}
      PROPERTIES DISABLED TRUE)
  endif()
endforeach()
