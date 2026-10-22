if(NOT DEFINED REX_SOURCE_DIR)
  message(FATAL_ERROR "REX_SOURCE_DIR is not set")
endif()

set(_forbidden_paths
  "${REX_SOURCE_DIR}/configure.ac"
  "${REX_SOURCE_DIR}/acmacros"
  "${REX_SOURCE_DIR}/config/Makefile.for.ROSE.includes.and.libs"
  "${REX_SOURCE_DIR}/config/QMTest_makefile.inc"
)

foreach(path IN LISTS _forbidden_paths)
  if(EXISTS "${path}")
    message(FATAL_ERROR "Autotools artifact detected: ${path}")
  endif()
endforeach()

set(_find_prune_expr
  -path "${REX_SOURCE_DIR}/.git"
  -o -path "${REX_SOURCE_DIR}/build"
  -o -path "${REX_SOURCE_DIR}/build-audit")
if(DEFINED REX_BUILD_DIR AND NOT REX_BUILD_DIR STREQUAL "")
  list(APPEND _find_prune_expr -o -path "${REX_BUILD_DIR}")
endif()

execute_process(
  COMMAND find "${REX_SOURCE_DIR}"
    "(" ${_find_prune_expr} ")" -prune -o
    "(" -name "Makefile.am*" -o -name "*.m4"
        -o -name "Makefile.in" -o -name "stamp-h*.in"
        -o -name "*_Testcodes*.cmake" -o -name "*tests_lists.cmake" ")"
    -type f -print -quit
  RESULT_VARIABLE _find_status
  OUTPUT_VARIABLE _autotools_file)

if(NOT _find_status STREQUAL "0")
  message(FATAL_ERROR "Failed to scan for Autotools artifacts (find exit ${_find_status})")
endif()

if(_autotools_file)
  message(FATAL_ERROR "Autotools artifact detected:\n  ${_autotools_file}")
endif()
