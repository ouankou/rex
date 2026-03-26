if(NOT DEFINED REX_SOURCE_DIR)
  message(FATAL_ERROR "REX_SOURCE_DIR is not set")
endif()

set(_forbidden_paths
  "${REX_SOURCE_DIR}/configure.ac"
  "${REX_SOURCE_DIR}/acmacros"
  "${REX_SOURCE_DIR}/config/Makefile.for.ROSE.includes.and.libs"
  "${REX_SOURCE_DIR}/config/QMTest_makefile.inc"
  "${REX_SOURCE_DIR}/config/Makefile.am"
)

set(_found "")
foreach(path IN LISTS _forbidden_paths)
  if(EXISTS "${path}")
    list(APPEND _found "${path}")
  endif()
endforeach()

set(_find_command find "${REX_SOURCE_DIR}"
  "(" -name "Makefile.am*" -o -name "*.m4" -o -name "Makefile.in" -o -name "stamp-h*.in" ")"
  -type f)

if(DEFINED REX_BUILD_DIR AND NOT REX_BUILD_DIR STREQUAL "")
  list(APPEND _find_command -not -path "${REX_BUILD_DIR}/*")
endif()
list(APPEND _find_command -not -path "${REX_SOURCE_DIR}/.git/*")

execute_process(
  COMMAND ${_find_command}
  RESULT_VARIABLE _find_status
  OUTPUT_VARIABLE _autotools_files
  OUTPUT_STRIP_TRAILING_WHITESPACE)

if(NOT _find_status EQUAL 0)
  message(FATAL_ERROR "Failed to scan for Autotools artifacts (find exit ${_find_status})")
endif()

string(REPLACE "\n" ";" _autotools_list "${_autotools_files}")
list(REMOVE_ITEM _autotools_list "")

if(_autotools_list)
  list(APPEND _found ${_autotools_list})
endif()

set(_legacy_test_bucket_pattern
  "[A-Z0-9_]*(REQUIRED_TO_PASS|VERIFIED_TO_PASS|ADDITIONAL_TO_RUN|LEGACY_DEFAULT|STANDARD_TO_RUN)")
file(GLOB_RECURSE _test_cmake_files
  "${REX_SOURCE_DIR}/tests/**/CMakeLists.txt"
  "${REX_SOURCE_DIR}/tests/**/*.cmake")
foreach(_cmake_file IN LISTS _test_cmake_files)
  file(READ "${_cmake_file}" _cmake_contents)
  string(REGEX MATCH "${_legacy_test_bucket_pattern}" _legacy_match "${_cmake_contents}")
  if(_legacy_match)
    list(APPEND _found "${_cmake_file}: ${_legacy_match}")
  endif()
endforeach()

file(GLOB_RECURSE _legacy_test_config_files
  "${REX_SOURCE_DIR}/tests/**/*_Testcodes*.cmake"
  "${REX_SOURCE_DIR}/tests/**/*tests_lists.cmake")
if(_legacy_test_config_files)
  list(APPEND _found ${_legacy_test_config_files})
endif()

if(_found)
  list(SORT _found)
  message(FATAL_ERROR "Autotools artifacts detected:\n  ${_found}")
endif()
