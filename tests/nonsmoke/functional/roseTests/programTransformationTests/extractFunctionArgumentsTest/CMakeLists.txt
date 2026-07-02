# Build the single statement to block normalization translator

include_directories(${CMAKE_SOURCE_DIR}/src/midend/programTransformation/extractFunctionArgumentsNormalization)
add_executable(normalizationTranslator NormalizationTranslator.C)
target_link_libraries(normalizationTranslator
  ROSE_DLL ${link_with_libraries})

set(TESTCODE_DIR ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)
include(${TESTCODE_DIR}/Cxx_transformation_specimens.cmake)

set(normalizationTranslator_SWITCHES
  -w -rose:verbose 0 -I${TESTCODE_DIR})

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

function(rex_configure_normalization_instrumented_test test_name)
  if(REX_EXTENDED_TEST_TIMEOUTS)
    set_tests_properties(${test_name} PROPERTIES
      PROCESSORS 16)
  endif()
endfunction()

set(_normalization_output_dir ${CMAKE_CURRENT_BINARY_DIR}/rose_test_output)
file(MAKE_DIRECTORY ${_normalization_output_dir})

set(_normalization_tests
  ${REX_CXX_TRANSFORMATION_TEST_SOURCES}
  test2005_50.C
)
list(REMOVE_DUPLICATES _normalization_tests)

foreach(file_to_test ${_normalization_tests})
  set(extra_frontend_flags)
  rex_get_cxx_specimen_extra_flags(extra_frontend_flags ${file_to_test})
  rex_get_cxx_specimen_standard_flag(_standard_flag ${file_to_test})
  list(APPEND extra_frontend_flags ${_standard_flag})
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
  set_tests_properties(normalizationTranslator_${file_to_test} PROPERTIES
    ENVIRONMENT "ROSE_TEST_OUTPUT_DIR=${_normalization_output_dir}")
  rex_configure_normalization_instrumented_test(normalizationTranslator_${file_to_test})
endforeach()
