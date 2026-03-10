# Build the single statement to block normalization translator

include_directories(${CMAKE_SOURCE_DIR}/src/midend/programTransformation/extractFunctionArgumentsNormalization)
add_executable(normalizationTranslator NormalizationTranslator.C)
target_link_libraries(normalizationTranslator
  ROSE_DLL ${link_with_libraries})

set(TESTCODE_DIR ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)
include(${TESTCODE_DIR}/Cxx_transformation_specimens.cmake)
set(NORMALIZATION_SYSTEM_HEADER_DIR
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/input_codes/system_header_like)

set(normalizationTranslator_SWITCHES
  -w -rose:verbose 0 -I${TESTCODE_DIR})

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

set(_normalization_output_dir ${CMAKE_CURRENT_BINARY_DIR}/rose_test_output)
file(MAKE_DIRECTORY ${_normalization_output_dir})

set(_normalization_tests
  ${REX_CXX_TRANSFORMATION_TEST_SOURCES}
  test2005_50.C
)
list(REMOVE_DUPLICATES _normalization_tests)

set(_normalization_test_targets)
foreach(file_to_test ${_normalization_tests})
  set(extra_frontend_flags "")
  if (file_to_test STREQUAL "rex_test2025_issue148_system_header_mutation.cpp")
    list(APPEND extra_frontend_flags -isystem ${NORMALIZATION_SYSTEM_HEADER_DIR})
  endif()
  if (file_to_test STREQUAL "rex_test2026_concepts.cpp")
    list(APPEND extra_frontend_flags -std=c++20)
  endif()
  if (file_to_test IN_LIST REX_FRONTEND_ERROR_TESTS)
    list(APPEND extra_frontend_flags -rex:clang:continue-on-error)
  endif()
  string(JOIN " " _normalization_switches ${normalizationTranslator_SWITCHES})
  string(JOIN " " _extra_frontend_flags ${extra_frontend_flags})
  set(_normalization_cmd
    "$<TARGET_FILE:normalizationTranslator> ${_normalization_switches} ${_extra_frontend_flags} -c ${TESTCODE_DIR}/${file_to_test}")
  string(STRIP "${_normalization_cmd}" _normalization_cmd)
  add_test(
    NAME normalizationTranslator_${file_to_test}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${_rth}
      CMD="${_normalization_cmd}"
      ${_exit_status}
      normalizationTranslator_${file_to_test}.passed
  )
  list(APPEND _normalization_test_targets normalizationTranslator_${file_to_test})
  set_tests_properties(normalizationTranslator_${file_to_test} PROPERTIES
    ENVIRONMENT "ROSE_TEST_OUTPUT_DIR=${_normalization_output_dir}")
endforeach()

# Makefile check targets.
set(_single_stmt_test_targets)
foreach(specimen IN LISTS REX_CXX_TRANSFORMATION_TEST_SOURCES)
  list(APPEND _single_stmt_test_targets singleStatementToBlockNormalization_${specimen})
endforeach()
set(_extract_args_group_tests ${_normalization_test_targets} ${_single_stmt_test_targets})
list(JOIN _extract_args_group_tests "\;" _extract_args_group_tests_escaped)
add_test(
  NAME nonsmoke_functional_roseTests_programTransformationTests_extractFunctionArgumentsTest_check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=${_extract_args_group_tests_escaped}
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(
  nonsmoke_functional_roseTests_programTransformationTests_extractFunctionArgumentsTest_check-local
  PROPERTIES DEPENDS "${_extract_args_group_tests}"
)
