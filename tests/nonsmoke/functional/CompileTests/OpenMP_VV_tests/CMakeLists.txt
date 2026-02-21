find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; OpenMP_VV_tests require bash")
endif()

if(NOT TARGET parseOmp)
  message(FATAL_ERROR "OpenMP_VV_tests require parseOmp target from OpenMP_tests")
endif()

set(OMPVV_SOURCE_ROOT "${CMAKE_SOURCE_DIR}/third_party/OpenMP_VV/tests/5.0")
set(OMPVV_REFERENCE_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/referenceResults/5.0")
set(OMPVV_EXTRACT_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/extract_omp_directives.sh")
set(OMPVV_RUN_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/run_ompvv_5_0_case.sh")
set(OMPVV_SUPPORT_ROOT "${CMAKE_SOURCE_DIR}/third_party/OpenMP_VV/ompvv")
set(OMPVV_OUTPUT_ROOT "${CMAKE_CURRENT_BINARY_DIR}/test-output/5.0")

if(NOT EXISTS "${OMPVV_SOURCE_ROOT}")
  message(FATAL_ERROR "OpenMP_VV source tree not found: ${OMPVV_SOURCE_ROOT}")
endif()

if(NOT EXISTS "${OMPVV_EXTRACT_SCRIPT}")
  message(FATAL_ERROR "OpenMP_VV extractor script missing: ${OMPVV_EXTRACT_SCRIPT}")
endif()

if(NOT EXISTS "${OMPVV_RUN_SCRIPT}")
  message(FATAL_ERROR "OpenMP_VV runner script missing: ${OMPVV_RUN_SCRIPT}")
endif()

file(GLOB_RECURSE OMPVV_5_0_TESTS
     RELATIVE "${OMPVV_SOURCE_ROOT}"
     "${OMPVV_SOURCE_ROOT}/*.c"
     "${OMPVV_SOURCE_ROOT}/*.cc"
     "${OMPVV_SOURCE_ROOT}/*.cpp"
     "${OMPVV_SOURCE_ROOT}/*.cxx"
     "${OMPVV_SOURCE_ROOT}/*.C"
     "${OMPVV_SOURCE_ROOT}/*.f"
     "${OMPVV_SOURCE_ROOT}/*.F"
     "${OMPVV_SOURCE_ROOT}/*.f90"
     "${OMPVV_SOURCE_ROOT}/*.F90")
list(SORT OMPVV_5_0_TESTS)

if(OMPVV_5_0_TESTS STREQUAL "")
  message(FATAL_ERROR "No OpenMP_VV 5.0 test sources found under ${OMPVV_SOURCE_ROOT}")
endif()

file(MAKE_DIRECTORY "${OMPVV_OUTPUT_ROOT}")

foreach(file_to_test IN LISTS OMPVV_5_0_TESTS)
  set(_source_file "${OMPVV_SOURCE_ROOT}/${file_to_test}")
  set(_reference_file "${OMPVV_REFERENCE_ROOT}/${file_to_test}.output")

  if(NOT EXISTS "${_reference_file}")
    message(FATAL_ERROR "Missing OpenMP_VV reference output: ${_reference_file}")
  endif()

  get_filename_component(_work_subdir "${file_to_test}" DIRECTORY)
  get_filename_component(_source_basename "${file_to_test}" NAME)
  get_filename_component(_source_extension "${_source_file}" EXT)

  set(_workdir "${OMPVV_OUTPUT_ROOT}/${_work_subdir}")
  file(MAKE_DIRECTORY "${_workdir}")

  set(_rose_file "rose_${_source_basename}")
  set(_output_file "${_rose_file}.output")
  set(_diff_file "${_output_file}.diff")

  set(_safe_name "${file_to_test}")
  string(REPLACE "/" "_" _safe_name "${_safe_name}")
  string(REPLACE "." "_" _safe_name "${_safe_name}")
  string(REPLACE "-" "_" _safe_name "${_safe_name}")

  set(_rose_flags
      -rose:openmp:ast_only
      -rose:skipfinalCompileStep
      -w
      -rose:verbose 0)

  if(_source_extension STREQUAL ".cpp"
      OR _source_extension STREQUAL ".cc"
      OR _source_extension STREQUAL ".cxx"
      OR _source_extension STREQUAL ".C")
    list(APPEND _rose_flags -std=c++17)
  elseif(_source_extension STREQUAL ".f"
      OR _source_extension STREQUAL ".F"
      OR _source_extension STREQUAL ".f90"
      OR _source_extension STREQUAL ".F90")
    list(APPEND _rose_flags -cpp)
  endif()

  add_test(
    NAME OMPVV_5_0_${_safe_name}
    WORKING_DIRECTORY "${_workdir}"
    COMMAND ${BASH_EXECUTABLE} "${OMPVV_RUN_SCRIPT}"
            "$<TARGET_FILE:parseOmp>"
            "${OMPVV_EXTRACT_SCRIPT}"
            "${_source_file}"
            "${_reference_file}"
            "${_rose_file}"
            "${_output_file}"
            "${_diff_file}"
            "${OMPVV_SOURCE_ROOT}"
            "${OMPVV_SUPPORT_ROOT}"
            "${_source_extension}"
            ${_rose_flags})
  set_tests_properties(OMPVV_5_0_${_safe_name}
    PROPERTIES LABELS "OMPVV;OMPVV_5_0")
endforeach()

list(LENGTH OMPVV_5_0_TESTS _ompvv_5_0_count)
message(STATUS "Configured OpenMP_VV 5.0 tests: ${_ompvv_5_0_count} source files")
