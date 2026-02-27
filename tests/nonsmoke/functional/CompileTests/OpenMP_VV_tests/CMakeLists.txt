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
set(OMPVV_SUPPORT_ROOT "${CMAKE_SOURCE_DIR}/third_party/OpenMP_VV/ompvv")

if(NOT EXISTS "${OMPVV_EXTRACT_SCRIPT}")
  message(FATAL_ERROR "OpenMP_VV extractor script missing: ${OMPVV_EXTRACT_SCRIPT}")
endif()

if(NOT EXISTS "${OMPVV_RUN_SCRIPT}")
  message(FATAL_ERROR "OpenMP_VV runner script missing: ${OMPVV_RUN_SCRIPT}")
endif()

if(NOT EXISTS "${OMPVV_SUPPORT_ROOT}")
  message(FATAL_ERROR "OpenMP_VV support tree not found: ${OMPVV_SUPPORT_ROOT}")
endif()

function(add_ompvv_version_tests version)
  string(REPLACE "." "_" version_tag "${version}")

  set(source_root "${CMAKE_SOURCE_DIR}/third_party/OpenMP_VV/tests/${version}")
  set(reference_root "${CMAKE_CURRENT_SOURCE_DIR}/referenceResults/${version}")
  set(output_root "${CMAKE_CURRENT_BINARY_DIR}/test-output/${version}")

  if(NOT EXISTS "${source_root}")
    message(FATAL_ERROR "OpenMP_VV source tree not found: ${source_root}")
  endif()

  if(NOT EXISTS "${reference_root}")
    message(FATAL_ERROR "OpenMP_VV reference tree not found: ${reference_root}")
  endif()

  file(GLOB_RECURSE version_tests
       RELATIVE "${source_root}"
       "${source_root}/*.c"
       "${source_root}/*.cc"
       "${source_root}/*.cpp"
       "${source_root}/*.cxx"
       "${source_root}/*.C"
       "${source_root}/*.f"
       "${source_root}/*.F"
       "${source_root}/*.f90"
       "${source_root}/*.F90")
  list(SORT version_tests)

  if(version_tests STREQUAL "")
    message(FATAL_ERROR "No OpenMP_VV ${version} test sources found under ${source_root}")
  endif()

  file(MAKE_DIRECTORY "${output_root}")

  foreach(file_to_test IN LISTS version_tests)
    set(source_file "${source_root}/${file_to_test}")
    set(reference_file "${reference_root}/${file_to_test}.output")

    if(NOT EXISTS "${reference_file}")
      message(FATAL_ERROR "Missing OpenMP_VV ${version} reference output: ${reference_file}")
    endif()

    get_filename_component(work_subdir "${file_to_test}" DIRECTORY)
    get_filename_component(source_basename "${file_to_test}" NAME)
    get_filename_component(source_extension "${source_file}" EXT)

    set(workdir "${output_root}/${work_subdir}")
    file(MAKE_DIRECTORY "${workdir}")

    set(rose_file "rose_${source_basename}")
    set(output_file "${rose_file}.output")
    set(diff_file "${output_file}.diff")

    set(safe_name "${file_to_test}")
    string(REPLACE "/" "_" safe_name "${safe_name}")
    string(REPLACE "." "_" safe_name "${safe_name}")
    string(REPLACE "-" "_" safe_name "${safe_name}")

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
      PROPERTIES LABELS "OMPVV;OMPVV_${version_tag}")
  endforeach()

  list(LENGTH version_tests ompvv_count)
  message(STATUS "Configured OpenMP_VV ${version} tests: ${ompvv_count} source files")
endfunction()

add_ompvv_version_tests("5.0")
add_ompvv_version_tests("4.5")
add_ompvv_version_tests("5.1")
add_ompvv_version_tests("5.2")
