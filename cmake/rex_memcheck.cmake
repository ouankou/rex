# Configure memory checking after resolving the exact LLVM/backend drivers.
# Valgrind matches the executed pathname, so preserve both each configured
# language-driver alias and its canonical path. A missing alias used to trace
# Clang++ and its linker even though the backend compiler boundary was excluded.
# Interpreters, REX translators and produced programs remain instrumented.

set(ROSE_CTEST_EXPLICIT_TIMEOUT_SCALE 1)
set(ROSE_CTEST_RTH_TIMEOUT_SCALE 1)
if(ROSE_VALGRIND_REQUESTED)
  set(ROSE_CTEST_EXPLICIT_TIMEOUT_SCALE 4)
  set(ROSE_CTEST_RTH_TIMEOUT_SCALE 4)
  if(REX_EXTENDED_TEST_TIMEOUTS)
    set(ROSE_CTEST_RTH_TIMEOUT_SCALE 24)
  endif()
endif()

set(ROSE_CTEST_MEMCHECK_COMMAND "")
set(ROSE_CTEST_MEMCHECK_OPTIONS "")
set(ROSE_CTEST_MEMCHECK_SUPPRESSIONS "")
set(ROSE_CTEST_MEMCHECK_TYPE "")
if(ROSE_VALGRIND_REQUESTED)
  find_program(REX_TEST_PYTHON_EXECUTABLE NAMES python3 REQUIRED)
  set(ROSE_REAL_VALGRIND "${VALGRIND_BINARY}")
  set(ROSE_CTEST_MEMCHECK_RTH_TIMEOUT_SCALE ${ROSE_CTEST_RTH_TIMEOUT_SCALE})
  # EXT-VALGRIND-MULTICALL-ENV: see docs/codebase-audit/external-exceptions.md.
  if(NOT EXISTS "/usr/bin/env")
    message(FATAL_ERROR
      "CTest MemCheck requires the absolute shebang interpreter /usr/bin/env.")
  endif()
  find_program(BASH_EXECUTABLE NAMES bash REQUIRED)
  execute_process(
    COMMAND "${VALGRIND_BINARY}" --quiet --tool=none --trace-children=yes
      "${BASH_EXECUTABLE}" -c
      [=[exec -a rex-memcheck-env-check /usr/bin/env "$@"]=]
      rex-memcheck-env-check -S "-u REX_MEMCHECK_ENV_PROBE"
      "${CMAKE_COMMAND}" -E true
    RESULT_VARIABLE _rose_memcheck_env_status
    OUTPUT_QUIET
    ERROR_VARIABLE _rose_memcheck_env_error)
  if(NOT _rose_memcheck_env_status STREQUAL "0")
    message(FATAL_ERROR
      "CTest MemCheck requires /usr/bin/env to support -S and the synthetic "
      "argv[0] supplied to traced script interpreters. Install a compatible "
      "standalone env utility, such as Ubuntu's coreutils-from-gnu alternative. "
      "Probe failed: ${_rose_memcheck_env_error}")
  endif()
  unset(_rose_memcheck_env_status)
  unset(_rose_memcheck_env_error)
  function(_rose_resolve_memcheck_compiler _rose_compiler _rose_output)
    if(NOT _rose_compiler OR NOT IS_ABSOLUTE "${_rose_compiler}" OR
       NOT EXISTS "${_rose_compiler}")
      message(FATAL_ERROR
        "CTest memcheck requires an existing absolute compiler path, got '${_rose_compiler}'.")
    endif()
    file(REAL_PATH "${_rose_compiler}" _rose_compiler_real)
    set(${_rose_output} "${_rose_compiler_real}" PARENT_SCOPE)
  endfunction()
  _rose_resolve_memcheck_compiler("${CMAKE_C_COMPILER}"
    ROSE_CTEST_MEMCHECK_C_COMPILER)
  _rose_resolve_memcheck_compiler("${CMAKE_CXX_COMPILER}"
    ROSE_CTEST_MEMCHECK_CXX_COMPILER)
  set(ROSE_CTEST_MEMCHECK_FORTRAN_COMPILER "")
  if(CMAKE_Fortran_COMPILER)
    _rose_resolve_memcheck_compiler("${CMAKE_Fortran_COMPILER}"
      ROSE_CTEST_MEMCHECK_FORTRAN_COMPILER)
  endif()
  set(ROSE_CTEST_MEMCHECK_WRAPPER
      "${CMAKE_BINARY_DIR}/scripts/rex-ctest-valgrind-wrapper")
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/scripts")
  configure_file(cmake/rex-ctest-valgrind-wrapper.in
                 "${ROSE_CTEST_MEMCHECK_WRAPPER}" @ONLY)
  file(CHMOD "${ROSE_CTEST_MEMCHECK_WRAPPER}"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
  set(ROSE_CTEST_MEMCHECK_COMMAND "${ROSE_CTEST_MEMCHECK_WRAPPER}")
  set(ROSE_CTEST_MEMCHECK_TYPE "Valgrind")
  set(ROSE_CTEST_MEMCHECK_SUPPRESSIONS "${CMAKE_SOURCE_DIR}/scripts/rex-suppressions-for-valgrind")
  # Skipping an interpreter also skips every descendant it launches. Shells,
  # RTH, build tools, and Python may all launch REX processes and stay traced.
  # Exact backend compilers and the documented tool protocols below have
  # independent execution boundaries.
  set(_rose_memcheck_trace_children_skip)
  foreach(_rose_compiler IN ITEMS
      "${CMAKE_C_COMPILER}" "${CMAKE_CXX_COMPILER}"
      "${CMAKE_Fortran_COMPILER}"
      "${ROSE_LLVM_CLANG_C_DRIVER}" "${ROSE_LLVM_CLANG_CXX_DRIVER}")
    if(_rose_compiler)
      list(APPEND _rose_memcheck_trace_children_skip "${_rose_compiler}")
      file(REAL_PATH "${_rose_compiler}" _rose_compiler_real)
      list(APPEND _rose_memcheck_trace_children_skip "${_rose_compiler_real}")
    endif()
  endforeach()
  # EXT-VALGRIND-NESTED-LAUNCHER (docs/codebase-audit/external-exceptions.md):
  # a test may run its own Memcheck contracts. The exact configured
  # Valgrind launcher starts its own instrumentation; tracing that launcher
  # inside another Valgrind can fail before the inner checker maps its runtime.
  # Keep the test's interpreter and its other children under the outer checker.
  # rex_memcheck_external_boundary verifies the inner checker's real reports.
  file(REAL_PATH "${ROSE_REAL_VALGRIND}" _rose_memcheck_valgrind_real)
  list(APPEND _rose_memcheck_trace_children_skip
    "${ROSE_REAL_VALGRIND}" "${_rose_memcheck_valgrind_real}")
  unset(_rose_memcheck_valgrind_real)
  list(REMOVE_DUPLICATES _rose_memcheck_trace_children_skip)
  list(JOIN _rose_memcheck_trace_children_skip "," _rose_memcheck_trace_children_skip_arg)
  set(ROSE_CTEST_MEMCHECK_OPTIONS "--leak-check=full --show-leak-kinds=definite,indirect,possible --errors-for-leak-kinds=definite,indirect,possible --error-exitcode=1 --max-stackframe=2047456 --num-callers=32 --trace-children=yes --trace-children-skip=${_rose_memcheck_trace_children_skip_arg} --trace-children-skip-by-arg=*gtest_internal_run_death_test*")
  unset(_rose_compiler)
  unset(_rose_compiler_real)
  unset(_rose_memcheck_trace_children_skip_arg)
  unset(_rose_memcheck_trace_children_skip)
  if(NOT EXISTS "${ROSE_CTEST_MEMCHECK_SUPPRESSIONS}")
    message(FATAL_ERROR
      "Required Valgrind report policy is missing: ${ROSE_CTEST_MEMCHECK_SUPPRESSIONS}")
  endif()
endif()
set(CTEST_MEMORYCHECK_COMMAND "${ROSE_CTEST_MEMCHECK_COMMAND}")
set(CTEST_MEMORYCHECK_COMMAND_OPTIONS "${ROSE_CTEST_MEMCHECK_OPTIONS}")
set(CTEST_MEMORYCHECK_SUPPRESSIONS_FILE "${ROSE_CTEST_MEMCHECK_SUPPRESSIONS}")
set(ROSE_CTEST_CUSTOM_ENVIRONMENT "")
if(ROSE_VALGRIND_REQUESTED)
  set(ROSE_CTEST_CUSTOM_ENVIRONMENT
      "set(CTEST_CUSTOM_ENVIRONMENT \"ROSE_TEST_TIMEOUT_SCALE=${ROSE_CTEST_RTH_TIMEOUT_SCALE}\")")
  # Memcheck can exceed CTest's default 1500-second timeout on parser-heavy tests.
  set(_rose_dashboard_timeout 5400)
  if(REX_EXTENDED_TEST_TIMEOUTS)
    set(_rose_dashboard_timeout 22200)
  endif()
  set(DART_TESTING_TIMEOUT ${_rose_dashboard_timeout} CACHE STRING
      "Maximum time in seconds for CTest dashboard tests" FORCE)
  unset(_rose_dashboard_timeout)
endif()
set(MEMORYCHECK_COMMAND "${ROSE_CTEST_MEMCHECK_COMMAND}" CACHE FILEPATH
    "Path to the memory checking command used by CTest" FORCE)
set(MEMORYCHECK_TYPE "${ROSE_CTEST_MEMCHECK_TYPE}" CACHE STRING
    "Memory checker type used by CTest" FORCE)
set(MEMORYCHECK_COMMAND_OPTIONS "${ROSE_CTEST_MEMCHECK_OPTIONS}" CACHE STRING
    "Command line options for the CTest memory checker" FORCE)
set(MEMORYCHECK_SUPPRESSIONS_FILE "${ROSE_CTEST_MEMCHECK_SUPPRESSIONS}" CACHE FILEPATH
    "Suppressions file used by the CTest memory checker" FORCE)
include(CTest)
