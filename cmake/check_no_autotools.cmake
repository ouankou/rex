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

if(_found)
  list(SORT _found)
  message(FATAL_ERROR "Autotools artifacts detected:\n  ${_found}")
endif()
