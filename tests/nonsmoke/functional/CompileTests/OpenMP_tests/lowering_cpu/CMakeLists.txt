set(OMP_LOWERING_CPU_EXACT_CASES
    barrier.c
    parallel.c
    parallel-if.c
    parallel-if-numthreads.c
    parallel-numthreads.c
    parallel-reduction.c
    parallel-reduction2.c
    parallelfor2.c
    rex_omp_collapse_constant_expression.c
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

set(_omp_cpu_compiler "${ROSE_LLVM_CLANG_C_DRIVER}")
set(_llvm_omp_header "${ROSE_LLVM_OPENMP_INCLUDE_DIR}/omp.h")
set(_iomp_dir "${ROSE_LLVM_OPENMP_RUNTIME_DIR}")
if(NOT _omp_cpu_compiler OR NOT EXISTS "${_omp_cpu_compiler}" OR
   NOT ROSE_LLVM_OPENMP_INCLUDE_DIR OR NOT EXISTS "${_llvm_omp_header}" OR
   NOT _iomp_dir OR NOT IS_DIRECTORY "${_iomp_dir}" OR
   NOT ROSE_LLVM_OPENMP_RUNTIME_LIBRARY OR
   NOT EXISTS "${ROSE_LLVM_OPENMP_RUNTIME_LIBRARY}")
  message(FATAL_ERROR
    "OpenMP lowering CPU tests require the configured LLVM driver, header and runtime")
endif()

# Stage only omp.h so include lookup still uses the active compiler's std headers.
set(_omp_header_stage_dir "${CMAKE_CURRENT_BINARY_DIR}/llvm_openmp_include")
file(MAKE_DIRECTORY "${_omp_header_stage_dir}")
configure_file("${_llvm_omp_header}" "${_omp_header_stage_dir}/omp.h" COPYONLY)

set(_cpu_compare_script "${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_cpu_compare.sh")
set(_omp_cpu_test_output_dir "${CMAKE_CURRENT_BINARY_DIR}/test-output")
file(MAKE_DIRECTORY "${_omp_cpu_test_output_dir}")

set(_omp_cpu_sanitizer_test_environment)
if(ROSE_USE_SANITIZER AND ROSE_SANITIZERS)
  string(REPLACE ";" "," _omp_cpu_sanitizer_test_list "${ROSE_SANITIZERS}")
  list(APPEND _omp_cpu_sanitizer_test_environment
       "ROSE_TEST_SANITIZER_FLAGS=-fsanitize=${_omp_cpu_sanitizer_test_list}")
endif()

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
            "${ROSE_LLVM_OPENMP_RUNTIME_LIBRARY}"
            "${CMAKE_SOURCE_DIR}/src/midend/programTransformation/ompLowering"
            "${input_file}"
            "${ROSE_OPENMP_VERSION}")

  set_tests_properties(
    OMPLOWERING_CPU_${_name}
    PROPERTIES
      LABELS "OMPLOWERING;OMPLOWERING_CPU"
      TIMEOUT 300)
  if(_omp_cpu_sanitizer_test_environment)
    set_property(TEST OMPLOWERING_CPU_${_name} APPEND PROPERTY ENVIRONMENT
                 "${_omp_cpu_sanitizer_test_environment}")
  endif()
endfunction()

foreach(_cpu_case IN LISTS OMP_LOWERING_CPU_EXACT_CASES)
  add_omp_lowering_cpu_compare_test(${_cpu_case} exact)
endforeach()

foreach(_cpu_case IN LISTS OMP_LOWERING_CPU_SORTED_CASES)
  add_omp_lowering_cpu_compare_test(${_cpu_case} sort)
endforeach()
