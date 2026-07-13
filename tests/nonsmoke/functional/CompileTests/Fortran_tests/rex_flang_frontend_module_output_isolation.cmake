if(NOT DEFINED TEST_TRANSLATOR OR TEST_TRANSLATOR STREQUAL "")
  message(FATAL_ERROR "TEST_TRANSLATOR is required")
endif()
if(NOT DEFINED SPECIMEN OR SPECIMEN STREQUAL "")
  message(FATAL_ERROR "SPECIMEN is required")
endif()
if(NOT DEFINED OUTPUT_DIR OR OUTPUT_DIR STREQUAL "")
  message(FATAL_ERROR "OUTPUT_DIR is required")
endif()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(_rose_output
    "${OUTPUT_DIR}/rose_rex_flang_frontend_module_output_isolation.f90")

execute_process(
  COMMAND
    "${TEST_TRANSLATOR}"
    -rose:verbose 0
    -rose:skipfinalCompileStep
    -rose:fortran_std=f2008
    -rose:output "${_rose_output}"
    -c "${SPECIMEN}"
  WORKING_DIRECTORY "${OUTPUT_DIR}"
  RESULT_VARIABLE _translator_status
  OUTPUT_VARIABLE _translator_stdout
  ERROR_VARIABLE _translator_stderr)

if(NOT _translator_status EQUAL 0)
  message(
    FATAL_ERROR
      "Flang frontend failed with status ${_translator_status}\n"
      "stdout:\n${_translator_stdout}\n"
      "stderr:\n${_translator_stderr}")
endif()
if(NOT EXISTS "${_rose_output}")
  message(FATAL_ERROR "Flang frontend did not produce ${_rose_output}")
endif()

set(_source_module_file
    "${OUTPUT_DIR}/rex_flang_frontend_module_output_isolation.mod")
if(NOT EXISTS "${_source_module_file}")
  message(
    FATAL_ERROR
      "Flang semantic analysis did not produce the source-defined module "
      "${_source_module_file}")
endif()

file(
  GLOB_RECURSE _module_files
  LIST_DIRECTORIES false
  "${OUTPUT_DIR}/*.mod"
  "${OUTPUT_DIR}/*.rmod"
  "${OUTPUT_DIR}/*.rcmp")
list(REMOVE_ITEM _module_files "${_source_module_file}")
if(_module_files)
  list(JOIN _module_files "\n  " _module_file_list)
  message(
    FATAL_ERROR
      "Nested Flang analysis of imported module files leaked rewritten modules "
      "outside its invocation-owned temporary directory:\n  "
      "${_module_file_list}")
endif()
