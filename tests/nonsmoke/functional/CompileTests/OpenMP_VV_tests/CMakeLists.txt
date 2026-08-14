find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; OpenMP_VV_tests require bash")
endif()

if(NOT TARGET parseOmp)
  message(FATAL_ERROR "OpenMP_VV_tests require parseOmp target from OpenMP_tests")
endif()

set(OMPVV_EXTRACT_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/extract_omp_directives.sh")
set(OMPVV_RUN_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/run_ompvv_5_0_case.sh")
set(OMPVV_NEGATIVE_PARSE_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/rex_ompvv_parse_must_fail.sh")
set(OMPVV_EXTRACTOR_CASE_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/run_ompvv_extractor_case.sh")
set(OMPVV_ROOT "${CMAKE_SOURCE_DIR}/tests/external/OpenMP_VV")
set(OMPVV_SUPPORT_ROOT "${OMPVV_ROOT}/ompvv")

# Keep malformed imported specimens as hard-negative coverage. These are
# rejected by the OpenMP 6.0 directive/clause applicability rules and must not
# be silently accepted merely to preserve an upstream positive-test label.
set(OMPVV_EXPECTED_INVALID_SOURCES
    "6.0/taskgraph/test_taskgraph_target.c")

if(NOT EXISTS "${OMPVV_EXTRACT_SCRIPT}")
  message(FATAL_ERROR "OpenMP_VV extractor script missing: ${OMPVV_EXTRACT_SCRIPT}")
endif()

if(NOT EXISTS "${OMPVV_RUN_SCRIPT}")
  message(FATAL_ERROR "OpenMP_VV runner script missing: ${OMPVV_RUN_SCRIPT}")
endif()

if(NOT EXISTS "${OMPVV_NEGATIVE_PARSE_SCRIPT}")
  message(FATAL_ERROR "OpenMP_VV negative parser script missing: ${OMPVV_NEGATIVE_PARSE_SCRIPT}")
endif()

if(NOT EXISTS "${OMPVV_EXTRACTOR_CASE_SCRIPT}")
  message(FATAL_ERROR "OpenMP_VV extractor test script missing: ${OMPVV_EXTRACTOR_CASE_SCRIPT}")
endif()

if(NOT EXISTS "${OMPVV_SUPPORT_ROOT}")
  message(FATAL_ERROR "OpenMP_VV support tree not found: ${OMPVV_SUPPORT_ROOT}")
endif()

set(OMPVV_NEGATIVE_OUTPUT_ROOT "${CMAKE_CURRENT_BINARY_DIR}/negative-test-output")
file(MAKE_DIRECTORY "${OMPVV_NEGATIVE_OUTPUT_ROOT}")
if(ENABLE-FORTRAN)
  add_test(
    NAME OMPVV_negative_bad_fortran_directive_parse_failure
    WORKING_DIRECTORY "${OMPVV_NEGATIVE_OUTPUT_ROOT}"
    COMMAND ${BASH_EXECUTABLE} "${OMPVV_NEGATIVE_PARSE_SCRIPT}"
            "$<TARGET_FILE:parseOmp>"
            "${CMAKE_CURRENT_SOURCE_DIR}/negative/bad_openmp_directive.F90"
            "REX_FLANG_INVARIANT[source-stream-directive]: REX OpenMP parser did not produce one typed Fortran directive"
            "  1:32: syntax error, unexpected EXPR_STRING")
  set_tests_properties(OMPVV_negative_bad_fortran_directive_parse_failure
    PROPERTIES LABELS "OMPVV;OMPVV_negative"
               ENVIRONMENT "ROSE_TEST_OUTPUT_DIR=${OMPVV_NEGATIVE_OUTPUT_ROOT}")
endif()

set(OMPVV_EXTRACTOR_OUTPUT_ROOT
    "${CMAKE_CURRENT_BINARY_DIR}/extractor-test-output")
file(MAKE_DIRECTORY "${OMPVV_EXTRACTOR_OUTPUT_ROOT}")
add_test(
  NAME OMPVV_rex_directive_extractor_continuation_reassembly
  WORKING_DIRECTORY "${OMPVV_EXTRACTOR_OUTPUT_ROOT}"
  COMMAND ${BASH_EXECUTABLE} "${OMPVV_EXTRACTOR_CASE_SCRIPT}"
          "${OMPVV_EXTRACT_SCRIPT}"
          "${CMAKE_CURRENT_SOURCE_DIR}/extractor/rex_openmp_vv_continuation_reassembly.F90"
          "${CMAKE_CURRENT_SOURCE_DIR}/extractor/rex_openmp_vv_continuation_reassembly.F90.output"
          "rex_openmp_vv_continuation_reassembly.F90.output")
set_tests_properties(OMPVV_rex_directive_extractor_continuation_reassembly
  PROPERTIES LABELS "OMPVV;OMPVV_extractor")

function(add_ompvv_version_tests version)
  string(REPLACE "." "_" version_tag "${version}")

  set(source_root "${OMPVV_ROOT}/tests/${version}")
  set(reference_root "${CMAKE_CURRENT_SOURCE_DIR}/referenceResults/${version}")
  set(output_root "${CMAKE_CURRENT_BINARY_DIR}/test-output/${version}")

  if(NOT EXISTS "${source_root}")
    message(FATAL_ERROR "OpenMP_VV source tree not found: ${source_root}")
  endif()

  if(NOT EXISTS "${reference_root}")
    message(FATAL_ERROR "OpenMP_VV reference tree not found: ${reference_root}")
  endif()

  set(version_tests)
  if(ENABLE-C)
    file(GLOB_RECURSE c_version_tests CONFIGURE_DEPENDS
         RELATIVE "${source_root}"
         "${source_root}/*.c"
         "${source_root}/*.cc"
         "${source_root}/*.cpp"
         "${source_root}/*.cxx"
         "${source_root}/*.C")
    list(APPEND version_tests ${c_version_tests})
  endif()
  if(ENABLE-FORTRAN)
    file(GLOB_RECURSE fortran_version_tests CONFIGURE_DEPENDS
         RELATIVE "${source_root}"
         "${source_root}/*.f"
         "${source_root}/*.F"
         "${source_root}/*.f90"
         "${source_root}/*.F90")
    list(APPEND version_tests ${fortran_version_tests})
  endif()
  list(SORT version_tests)

  if(version_tests STREQUAL "")
    message(FATAL_ERROR
      "No OpenMP_VV ${version} test sources found for the enabled REX languages under ${source_root}")
  endif()

  file(MAKE_DIRECTORY "${output_root}")

  foreach(file_to_test IN LISTS version_tests)
    set(source_file "${source_root}/${file_to_test}")
    set(reference_file "${reference_root}/${file_to_test}.output")

    if(NOT EXISTS "${reference_file}")
      message(FATAL_ERROR "Missing OpenMP_VV ${version} reference output: ${reference_file}")
    endif()

    get_filename_component(source_basename "${file_to_test}" NAME)
    get_filename_component(source_extension "${source_file}" EXT)
    get_filename_component(source_directory "${source_file}" DIRECTORY)

    set(safe_name "${file_to_test}")
    string(REPLACE "/" "_" safe_name "${safe_name}")
    string(REPLACE "." "_" safe_name "${safe_name}")
    string(REPLACE "-" "_" safe_name "${safe_name}")

    set(workdir "${output_root}/${safe_name}")
    file(MAKE_DIRECTORY "${workdir}")

    set(rose_file "rose_${source_basename}")
    set(output_file "${rose_file}.output")
    set(diff_file "${output_file}.diff")

    set(rose_flags
        -rose:openmp:ast_only
        -rose:skipfinalCompileStep
        -w
        -rose:verbose 0)

    if(source_extension STREQUAL ".cpp"
        OR source_extension STREQUAL ".cc"
        OR source_extension STREQUAL ".cxx"
        OR source_extension STREQUAL ".C")
      list(APPEND rose_flags -std=c++17)
    elseif(source_extension STREQUAL ".f"
        OR source_extension STREQUAL ".F"
        OR source_extension STREQUAL ".f90"
        OR source_extension STREQUAL ".F90")
      list(APPEND rose_flags -cpp)
    endif()

    set(versioned_source "${version}/${file_to_test}")
    if(versioned_source IN_LIST OMPVV_EXPECTED_INVALID_SOURCES)
      add_test(
        NAME OMPVV_${version_tag}_${safe_name}
        WORKING_DIRECTORY "${workdir}"
        COMMAND ${BASH_EXECUTABLE} "${OMPVV_NEGATIVE_PARSE_SCRIPT}"
                "$<TARGET_FILE:parseOmp>"
                "${source_file}"
                "REX_OMP_AST_INVARIANT[parser]: OpenMP parser rejected ' omp target nowait map(tofrom: y) depend(out: y) shared(y)'"
                "  1:50: syntax error, unexpected SHARED, expecting end of file"
                "-I${source_root}"
                "-I${OMPVV_SUPPORT_ROOT}"
                "-I${source_directory}"
                ${rose_flags})
      set_tests_properties(OMPVV_${version_tag}_${safe_name}
        PROPERTIES LABELS "OMPVV;OMPVV_${version_tag};OMPVV_negative"
                   ENVIRONMENT "ROSE_TEST_OUTPUT_DIR=${workdir}")
      continue()
    endif()

    add_test(
      NAME OMPVV_${version_tag}_${safe_name}
      WORKING_DIRECTORY "${workdir}"
      COMMAND ${BASH_EXECUTABLE} "${OMPVV_RUN_SCRIPT}"
              "$<TARGET_FILE:parseOmp>"
              "${OMPVV_EXTRACT_SCRIPT}"
              "${source_file}"
              "${reference_file}"
              "${rose_file}"
              "${output_file}"
              "${diff_file}"
              "${source_root}"
              "${OMPVV_SUPPORT_ROOT}"
              "${source_extension}"
              ${rose_flags})
    set_tests_properties(OMPVV_${version_tag}_${safe_name}
      PROPERTIES LABELS "OMPVV;OMPVV_${version_tag}"
                 ENVIRONMENT "ROSE_TEST_OUTPUT_DIR=${workdir}")
  endforeach()

  list(LENGTH version_tests ompvv_count)
  message(STATUS "Configured OpenMP_VV ${version} tests: ${ompvv_count} source files")
endfunction()

add_ompvv_version_tests("5.0")
add_ompvv_version_tests("4.5")
add_ompvv_version_tests("5.1")
add_ompvv_version_tests("5.2")
add_ompvv_version_tests("6.0")
