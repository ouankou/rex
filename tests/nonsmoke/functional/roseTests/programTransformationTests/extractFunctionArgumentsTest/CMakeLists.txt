# Build the single statement to block normalization translator

include_directories(${CMAKE_SOURCE_DIR}/src/midend/programTransformation/extractFunctionArgumentsNormalization)
add_executable(normalizationTranslator NormalizationTranslator.C)
target_link_libraries(normalizationTranslator
  ROSE_DLL ${link_with_libraries})

# This populates the list ROSE__CXX_TESTS

set(TESTCODE_DIR ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)
set(NORMALIZATION_SYSTEM_HEADER_DIR
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/input_codes/system_header_like)

set(normalizationTranslator_SWITCHES
	 -w -rose:verbose 0 -I${TESTCODE_DIR}
  -I${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/A++Code)

set(_rth ${CMAKE_SOURCE_DIR}/scripts/rth_run.pl)
set(_exit_status ${CMAKE_SOURCE_DIR}/scripts/test_exit_status)

set(_normalization_disabled_tests
  test2013_242.C
  test2013_241.C
)

set(_disable_test2005_50 FALSE)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS "6.1")
  set(_disable_test2005_50 TRUE)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(_disable_test2005_50 TRUE)
endif()

foreach(file_to_test ${ROSE__CXX_TESTS})
  set(extra_frontend_flags "")
  if (file_to_test STREQUAL "rex_test2025_issue148_system_header_mutation.cpp")
    list(APPEND extra_frontend_flags -isystem ${NORMALIZATION_SYSTEM_HEADER_DIR})
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
  if(file_to_test IN_LIST _normalization_disabled_tests)
    set_tests_properties(normalizationTranslator_${file_to_test} PROPERTIES DISABLED TRUE)
  endif()
  if(file_to_test STREQUAL "test2005_50.C" AND _disable_test2005_50)
    set_tests_properties(normalizationTranslator_${file_to_test} PROPERTIES DISABLED TRUE)
  endif()
endforeach()
