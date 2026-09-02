foreach(_required_variable ROSE_COMPILER SOURCE_FILE WORK_DIRECTORY EXPECTED_OUTPUT)
  if(NOT DEFINED ${_required_variable} OR "${${_required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${_required_variable} must be provided")
  endif()
endforeach()

if(NOT EXISTS "${ROSE_COMPILER}")
  message(FATAL_ERROR "rose-compiler does not exist: ${ROSE_COMPILER}")
endif()
if(NOT EXISTS "${SOURCE_FILE}")
  message(FATAL_ERROR "smoke-test source does not exist: ${SOURCE_FILE}")
endif()

file(REMOVE_RECURSE "${WORK_DIRECTORY}")
file(MAKE_DIRECTORY "${WORK_DIRECTORY}")
set(_output_file "${WORK_DIRECTORY}/rex_driver_compile_run")

execute_process(
  COMMAND "${ROSE_COMPILER}" "${SOURCE_FILE}" -o "${_output_file}"
  WORKING_DIRECTORY "${WORK_DIRECTORY}"
  RESULT_VARIABLE _compile_result
  OUTPUT_VARIABLE _compile_output
  ERROR_VARIABLE _compile_error)

if(NOT _compile_result EQUAL 0)
  file(REMOVE_RECURSE "${WORK_DIRECTORY}")
  message(FATAL_ERROR
    "rose-compiler failed with status ${_compile_result}\n"
    "stdout:\n${_compile_output}\n"
    "stderr:\n${_compile_error}")
endif()

execute_process(
  COMMAND "${_output_file}"
  WORKING_DIRECTORY "${WORK_DIRECTORY}"
  RESULT_VARIABLE _run_result
  OUTPUT_VARIABLE _run_output
  ERROR_VARIABLE _run_error)

set(_failure_message "")
if(NOT _run_result EQUAL 0)
  string(APPEND _failure_message
    "translated executable failed with status ${_run_result}\n"
    "stdout:\n${_run_output}\n"
    "stderr:\n${_run_error}")
else()
  string(FIND "${_run_output}" "${EXPECTED_OUTPUT}" _expected_output_position)
  if(_expected_output_position EQUAL -1)
    string(APPEND _failure_message
      "translated executable output did not contain '${EXPECTED_OUTPUT}'\n"
      "stdout:\n${_run_output}\n"
      "stderr:\n${_run_error}")
  endif()
endif()

file(REMOVE_RECURSE "${WORK_DIRECTORY}")
if(NOT _failure_message STREQUAL "")
  message(FATAL_ERROR "${_failure_message}")
endif()
