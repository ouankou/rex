include_directories(${ROSE_INCLUDES})
include_directories(
  ${CMAKE_SOURCE_DIR}/src/midend/programTransformation/singleStatementToBlockNormalization)

add_executable(singleStatementToBlockNormalization
  SingleStatementToBlockNormalization.C)
target_link_libraries(singleStatementToBlockNormalization
  ROSE_DLL ${link_with_libraries})

include(${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests/Cxx_transformation_specimens.cmake)

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

foreach(specimen IN LISTS REX_CXX_TRANSFORMATION_TEST_SOURCES)
  set(_single_stmt_target singleStatementToBlockNormalization_${specimen}.passed)
  rex_get_cxx_specimen_extra_flags(_single_stmt_extra_flags ${specimen})
  rex_get_cxx_specimen_standard_flag(_single_stmt_standard_flag ${specimen})
  list(APPEND _single_stmt_extra_flags ${_single_stmt_standard_flag})
  string(JOIN " " _single_stmt_extra_flags_str ${_single_stmt_extra_flags})
  set(_single_stmt_cmd
    "$<TARGET_FILE:singleStatementToBlockNormalization> ${_single_stmt_switches_str}")
  if(_single_stmt_extra_flags_str)
    string(APPEND _single_stmt_cmd " ${_single_stmt_extra_flags_str}")
  endif()
  string(APPEND _single_stmt_cmd " -c ${_single_stmt_tests_dir}/${specimen}")
  add_test(
    NAME singleStatementToBlockNormalization_${specimen}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="${_single_stmt_cmd}"
      ${_exit_status}
      ${_single_stmt_target}
  )
  set_tests_properties(singleStatementToBlockNormalization_${specimen} PROPERTIES
    ENVIRONMENT "ROSE_TEST_OUTPUT_DIR=${_single_stmt_output_dir}")
endforeach()

set(_single_stmt_issue432_output_dir
  ${CMAKE_CURRENT_BINARY_DIR}/issue432_explicit_instantiation_output)
add_test(
  NAME normalizationTranslator_test2005_155_explicit_instantiation_output
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_TRANSLATOR=${CMAKE_BINARY_DIR}/bin/normalizationTranslator
    -DREX_INPUT=${_single_stmt_tests_dir}/test2005_155.C
    -DREX_INCLUDE_DIR=${_single_stmt_tests_dir}
    -DREX_WORKDIR=${_single_stmt_issue432_output_dir}
    "-DREX_EXPECTED_REGEX=template[ ]+void[ ]+X::foo<int>\\(int i,int j,int k\\)"
    -P ${CMAKE_CURRENT_SOURCE_DIR}/check_unparsed_contains.cmake
)

set(_single_stmt_issue426_output_dir
  ${CMAKE_CURRENT_BINARY_DIR}/issue426_array_new_output)
add_test(
  NAME normalizationTranslator_test2005_38_array_new_output
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_TRANSLATOR=${CMAKE_BINARY_DIR}/bin/normalizationTranslator
    -DREX_INPUT=${_single_stmt_tests_dir}/test2005_38.C
    -DREX_INCLUDE_DIR=${_single_stmt_tests_dir}
    -DREX_WORKDIR=${_single_stmt_issue426_output_dir}
    "-DREX_EXPECTED_REGEX=new[ ]+double[ ]*\\[[ ]*100[ ]*\\]"
    -DREX_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    "-DREX_COMPILE_FLAGS=-std=c++17"
    -P ${CMAKE_CURRENT_SOURCE_DIR}/check_unparsed_contains.cmake
)
