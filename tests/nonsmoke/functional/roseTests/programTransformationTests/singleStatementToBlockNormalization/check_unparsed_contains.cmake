if(NOT DEFINED REX_TRANSLATOR)
  message(FATAL_ERROR "REX_TRANSLATOR is required")
endif()
if(NOT DEFINED REX_INPUT)
  message(FATAL_ERROR "REX_INPUT is required")
endif()
if(NOT DEFINED REX_INCLUDE_DIR)
  message(FATAL_ERROR "REX_INCLUDE_DIR is required")
endif()
if(NOT DEFINED REX_WORKDIR)
  message(FATAL_ERROR "REX_WORKDIR is required")
endif()
if(NOT DEFINED REX_EXPECTED_REGEX)
  message(FATAL_ERROR "REX_EXPECTED_REGEX is required")
endif()

get_filename_component(_input_name "${REX_INPUT}" NAME)
set(_output_file "${REX_WORKDIR}/rose_${_input_name}")

file(REMOVE_RECURSE "${REX_WORKDIR}")
file(MAKE_DIRECTORY "${REX_WORKDIR}")

execute_process(
  COMMAND "${REX_TRANSLATOR}" -w -rose:verbose 0 "-I${REX_INCLUDE_DIR}" -c "${REX_INPUT}"
  WORKING_DIRECTORY "${REX_WORKDIR}"
  RESULT_VARIABLE _translator_result
  OUTPUT_VARIABLE _translator_stdout
  ERROR_VARIABLE _translator_stderr
)

if(NOT _translator_result EQUAL 0)
  message(FATAL_ERROR
    "translator failed with exit code ${_translator_result}\n"
    "stdout:\n${_translator_stdout}\n"
    "stderr:\n${_translator_stderr}")
endif()

if(NOT EXISTS "${_output_file}")
  message(FATAL_ERROR "expected unparsed file not found: ${_output_file}")
endif()

file(READ "${_output_file}" _output_text)
string(REGEX MATCH "${REX_EXPECTED_REGEX}" _match "${_output_text}")
if(_match STREQUAL "")
  message(FATAL_ERROR
    "unparsed output did not match regex: ${REX_EXPECTED_REGEX}\n"
    "output file: ${_output_file}\n"
    "contents:\n${_output_text}")
endif()

if(DEFINED REX_CXX_COMPILER AND NOT REX_CXX_COMPILER STREQUAL "")
  set(_compile_flags)
  if(DEFINED REX_COMPILE_FLAGS AND NOT REX_COMPILE_FLAGS STREQUAL "")
    separate_arguments(_compile_flags NATIVE_COMMAND "${REX_COMPILE_FLAGS}")
  endif()

  execute_process(
    COMMAND "${REX_CXX_COMPILER}" ${_compile_flags} "-I${REX_INCLUDE_DIR}" -c "${_output_file}" -o "${REX_WORKDIR}/unparsed_output.o"
    WORKING_DIRECTORY "${REX_WORKDIR}"
    RESULT_VARIABLE _compile_result
    OUTPUT_VARIABLE _compile_stdout
    ERROR_VARIABLE _compile_stderr
  )

  if(NOT _compile_result EQUAL 0)
    message(FATAL_ERROR
      "compiling unparsed output failed with exit code ${_compile_result}\n"
      "output file: ${_output_file}\n"
      "stdout:\n${_compile_stdout}\n"
      "stderr:\n${_compile_stderr}")
  endif()
endif()
