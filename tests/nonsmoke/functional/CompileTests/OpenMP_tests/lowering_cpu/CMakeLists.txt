set(OMP_LOWERING_CPU_EXACT_CASES
    barrier.c
    parallel.c
    parallel-if.c
    parallel-if-numthreads.c
    parallel-numthreads.c
    parallel-reduction.c
    parallel-reduction2.c
    parallelfor2.c
    single.c
    omp_version.c)

set(OMP_LOWERING_CPU_SORTED_CASES
    ompfor.c
    ompfor_c99.c
    ompfor3.c
    ompfor4.c
    ompfor-default.c
    ompfor-decremental.c
    ompfor-static.c)

if(NOT BASH_EXECUTABLE)
  find_program(BASH_EXECUTABLE bash)
endif()

if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; lowering_cpu tests require bash")
endif()

if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
  message(WARNING "OpenMP lowering CPU compare tests require Clang; disabling lowering_cpu tests")
  return()
endif()

set(_omp_cpu_compiler "${CMAKE_C_COMPILER}")

# Prefer the LLVM OpenMP header resolved at configure time.
set(_llvm_omp_header "")
if(ROSE_LLVM_OPENMP_INCLUDE_DIR AND EXISTS "${ROSE_LLVM_OPENMP_INCLUDE_DIR}/omp.h")
  set(_llvm_omp_header "${ROSE_LLVM_OPENMP_INCLUDE_DIR}/omp.h")
endif()

execute_process(
  COMMAND "${_omp_cpu_compiler}" --print-resource-dir
  OUTPUT_VARIABLE _clang_resource_dir
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET)
if(NOT _llvm_omp_header AND _clang_resource_dir AND EXISTS "${_clang_resource_dir}/include/omp.h")
  set(_llvm_omp_header "${_clang_resource_dir}/include/omp.h")
endif()

if(NOT _llvm_omp_header)
  set(_omp_header_candidates "")

  if(LLVM_LIBRARY_DIRS)
    foreach(_llvm_lib_dir IN LISTS LLVM_LIBRARY_DIRS)
      file(GLOB _llvm_hdrs "${_llvm_lib_dir}/clang/*/include/omp.h")
      list(APPEND _omp_header_candidates ${_llvm_hdrs})
    endforeach()
  endif()

  if(LLVM_DIR)
    get_filename_component(_llvm_prefix_guess "${LLVM_DIR}/../../.." ABSOLUTE)
    file(GLOB _llvm_hdrs "${_llvm_prefix_guess}/lib/clang/*/include/omp.h")
    list(APPEND _omp_header_candidates ${_llvm_hdrs})
  endif()

  file(GLOB _sys_llvm_hdrs "/usr/lib/llvm-*/lib/clang/*/include/omp.h")
  list(APPEND _omp_header_candidates ${_sys_llvm_hdrs})

  list(REMOVE_DUPLICATES _omp_header_candidates)
  if(_omp_header_candidates)
    list(SORT _omp_header_candidates COMPARE NATURAL)
    list(GET _omp_header_candidates -1 _llvm_omp_header)
  endif()
endif()

if(NOT _llvm_omp_header OR NOT EXISTS "${_llvm_omp_header}")
  message(WARNING "Unable to locate LLVM OpenMP header (omp.h); disabling lowering_cpu tests")
  return()
endif()

# Prefer the LLVM OpenMP runtime resolved at configure time.
set(_iomp_dir "")
if(ROSE_LLVM_OPENMP_RUNTIME_DIR)
  if(EXISTS "${ROSE_LLVM_OPENMP_RUNTIME_DIR}/libiomp5.so" OR
     EXISTS "${ROSE_LLVM_OPENMP_RUNTIME_DIR}/libomp.so" OR
     EXISTS "${ROSE_LLVM_OPENMP_RUNTIME_DIR}/libiomp5.a" OR
     EXISTS "${ROSE_LLVM_OPENMP_RUNTIME_DIR}/libomp.a")
    set(_iomp_dir "${ROSE_LLVM_OPENMP_RUNTIME_DIR}")
  endif()
endif()
if(NOT _iomp_dir)
  execute_process(
    COMMAND "${_omp_cpu_compiler}" -fopenmp=libiomp5 -print-file-name=libiomp5.so
    OUTPUT_VARIABLE _iomp_lib
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(_iomp_lib AND NOT _iomp_lib STREQUAL "libiomp5.so" AND EXISTS "${_iomp_lib}")
    get_filename_component(_iomp_dir "${_iomp_lib}" DIRECTORY)
  else()
    execute_process(
      COMMAND "${_omp_cpu_compiler}" -fopenmp=libiomp5 -print-file-name=libomp.so
      OUTPUT_VARIABLE _iomp_lib
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
    if(_iomp_lib AND NOT _iomp_lib STREQUAL "libomp.so" AND EXISTS "${_iomp_lib}")
      get_filename_component(_iomp_dir "${_iomp_lib}" DIRECTORY)
    endif()
  endif()
  unset(_iomp_lib)
endif()
if(NOT _iomp_dir)
  message(WARNING "Unable to locate LLVM OpenMP runtime via ${_omp_cpu_compiler}; disabling lowering_cpu tests")
  return()
endif()

# Stage only omp.h so include lookup still uses the active compiler's std headers.
set(_omp_header_stage_dir "${CMAKE_CURRENT_BINARY_DIR}/llvm_openmp_include")
file(MAKE_DIRECTORY "${_omp_header_stage_dir}")
configure_file("${_llvm_omp_header}" "${_omp_header_stage_dir}/omp.h" COPYONLY)

set(_cpu_compare_script "${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_cpu_compare.sh")
set(_omp_cpu_test_output_dir "${CMAKE_CURRENT_BINARY_DIR}/test-output")
file(MAKE_DIRECTORY "${_omp_cpu_test_output_dir}")

function(add_omp_lowering_cpu_compare_test input_file mode)
  string(REPLACE "." "_" _name "${input_file}")
  string(REPLACE "-" "_" _name "${_name}")

  add_test(
    NAME OMPLOWERING_CPU_${_name}
    WORKING_DIRECTORY "${_omp_cpu_test_output_dir}"
    COMMAND "${BASH_EXECUTABLE}" "${_cpu_compare_script}"
            "$<TARGET_FILE:parseOmp>"
            "${_omp_cpu_compiler}"
            "${CMAKE_CURRENT_SOURCE_DIR}/../${input_file}"
            "${_omp_cpu_test_output_dir}/${_name}"
            "${mode}"
            "${_omp_header_stage_dir}"
            "${_iomp_dir}"
            "${CMAKE_SOURCE_DIR}/src/midend/programTransformation/ompLowering"
            "${input_file}"
            "${ROSE_OPENMP_VERSION}")

  set_tests_properties(
    OMPLOWERING_CPU_${_name}
    PROPERTIES
      LABELS "OMPLOWERING;OMPLOWERING_CPU"
      TIMEOUT 300)
endfunction()

foreach(_cpu_case IN LISTS OMP_LOWERING_CPU_EXACT_CASES)
  add_omp_lowering_cpu_compare_test(${_cpu_case} exact)
endforeach()

foreach(_cpu_case IN LISTS OMP_LOWERING_CPU_SORTED_CASES)
  add_omp_lowering_cpu_compare_test(${_cpu_case} sort)
endforeach()
