if(NOT DEFINED TEST_TRANSLATOR OR TEST_TRANSLATOR STREQUAL "")
  message(FATAL_ERROR "TEST_TRANSLATOR is required")
endif()

if(NOT DEFINED SPECIMEN OR SPECIMEN STREQUAL "")
  message(FATAL_ERROR "SPECIMEN is required")
endif()

if(NOT DEFINED ROSE_OUTPUT OR ROSE_OUTPUT STREQUAL "")
  message(FATAL_ERROR "ROSE_OUTPUT is required")
endif()

macro(escape_for_message_verbatim var)
  string(REPLACE "\\" "\\\\" ${var} "${${var}}")
  string(REPLACE "\"" "\\\"" ${var} "${${var}}")
  string(REPLACE "$" "$$" ${var} "${${var}}")
endmacro()

set(_translator_args -rose:experimental_flang_frontend)
if(DEFINED EXTRA_FLAGS_FILE)
  if(NOT EXISTS "${EXTRA_FLAGS_FILE}")
    message(FATAL_ERROR "EXTRA_FLAGS_FILE does not exist: ${EXTRA_FLAGS_FILE}")
  endif()
  include("${EXTRA_FLAGS_FILE}")
  list(APPEND _translator_args ${EXTRA_FLAGS_LIST})
endif()
list(APPEND _translator_args -c "${SPECIMEN}")

# Remove stale output so this test validates the current translator invocation.
file(REMOVE "${ROSE_OUTPUT}")

execute_process(
  COMMAND "${TEST_TRANSLATOR}" ${_translator_args}
  RESULT_VARIABLE _translator_status
  OUTPUT_VARIABLE _translator_stdout
  ERROR_VARIABLE _translator_stderr
)

if(NOT "${_translator_status}" STREQUAL "0")
  # Escape characters so translator output is reported verbatim by CMake message().
  escape_for_message_verbatim(_translator_stdout)
  escape_for_message_verbatim(_translator_stderr)
  message(
    FATAL_ERROR
      "testTranslator failed for ${SPECIMEN}\n"
      "exit code: ${_translator_status}\n"
      "stdout:\n${_translator_stdout}\n"
      "stderr:\n${_translator_stderr}"
  )
endif()

if(NOT EXISTS "${ROSE_OUTPUT}")
  message(FATAL_ERROR "Expected output ${ROSE_OUTPUT} was not generated for ${SPECIMEN}")
endif()

file(SIZE "${ROSE_OUTPUT}" _rose_output_size)
if(_rose_output_size EQUAL 0)
  message(FATAL_ERROR "Output ${ROSE_OUTPUT} is empty for ${SPECIMEN}")
endif()
