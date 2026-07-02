set(_omp_fortran_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/OpenMP_tests/fortran)

file(GLOB OMP_FORTRAN_ALL_CASES RELATIVE ${_omp_fortran_dir}
  ${_omp_fortran_dir}/*.f
  ${_omp_fortran_dir}/*.f90)
list(SORT OMP_FORTRAN_ALL_CASES)
set(_omp_fortran_lowering_output_dir
  ${CMAKE_CURRENT_BINARY_DIR}/test-output)
file(MAKE_DIRECTORY ${_omp_fortran_lowering_output_dir})

# Semantic runs execute lowered and original binaries multiple times and can
# legitimately run longer when the full OMP lowering suite is parallelized.
# The semantic harness itself downgrades to successful translation coverage
# when the original source is not semantically executable under the current
# Flang/OpenMP toolchain, so every specimen can be registered uniformly.
set(OMP_FORTRAN_SEMANTIC_RUN_TIMEOUT_SECONDS 180)
# run_fortran_semantic_compare.sh executes:
# 2 binaries (orig/lowered) * 2 thread counts * 3 repeats = 12 total runs.
set(OMP_FORTRAN_SEMANTIC_TOTAL_RUNS 12)
set(OMP_FORTRAN_SEMANTIC_COMPILE_OVERHEAD_SECONDS 360)

set(_omp_fortran_compiler "")
set(_omp_fortran_omp_include "")
set(_omp_fortran_missing_reqs)

if(enable-fortran)
  if(CMAKE_Fortran_COMPILER)
    set(_omp_fortran_compiler ${CMAKE_Fortran_COMPILER})
  else()
    find_program(_omp_fortran_compiler
      NAMES flang-${CLANG_VERSION_MAJOR} flang
      HINTS ${LLVM_TOOLS_BINARY_DIR})
  endif()
  if(NOT _omp_fortran_compiler)
    list(APPEND _omp_fortran_missing_reqs
         "Fortran compiler (set CMAKE_Fortran_COMPILER to flang)")
  endif()

  if(FLANG_INTRINSIC_INCLUDE_DIR AND EXISTS "${FLANG_INTRINSIC_INCLUDE_DIR}/omp_lib.h")
    set(_omp_fortran_omp_include "${FLANG_INTRINSIC_INCLUDE_DIR}")
  endif()
  if(NOT _omp_fortran_omp_include)
    foreach(_flang_include_dir IN LISTS FLANG_INCLUDE_DIRS)
      if(EXISTS "${_flang_include_dir}/flang/omp_lib.h")
        set(_omp_fortran_omp_include "${_flang_include_dir}/flang")
        break()
      endif()
    endforeach()
    unset(_flang_include_dir)
  endif()
  if(ROSE_LLVM_OPENMP_RUNTIME_DIR)
    get_filename_component(_llvm_runtime_prefix
      "${ROSE_LLVM_OPENMP_RUNTIME_DIR}/.." ABSOLUTE)
    if(EXISTS "${_llvm_runtime_prefix}/include/flang/omp_lib.h")
      set(_omp_fortran_omp_include "${_llvm_runtime_prefix}/include/flang")
    endif()
  endif()
  if(NOT _omp_fortran_omp_include AND _omp_fortran_compiler)
    get_filename_component(_flang_bin_dir "${_omp_fortran_compiler}" DIRECTORY)
    get_filename_component(_flang_prefix "${_flang_bin_dir}/.." ABSOLUTE)
    if(EXISTS "${_flang_prefix}/include/flang/omp_lib.h")
      set(_omp_fortran_omp_include "${_flang_prefix}/include/flang")
    endif()
  endif()
  if(NOT _omp_fortran_omp_include)
    list(APPEND _omp_fortran_missing_reqs
         "LLVM Fortran OpenMP include directory containing omp_lib.h")
  endif()

  if(NOT ROSE_LLVM_OPENMP_RUNTIME_DIR)
    list(APPEND _omp_fortran_missing_reqs
         "LLVM OpenMP runtime directory (ROSE_LLVM_OPENMP_RUNTIME_DIR)")
  endif()

  if(NOT TARGET kmpc_fortran_abi)
    list(APPEND _omp_fortran_missing_reqs "kmpc_fortran_abi target")
  endif()

  if(_omp_fortran_missing_reqs)
    string(REPLACE ";" "\n  - "
      _omp_fortran_missing_msg "${_omp_fortran_missing_reqs}")
    message(FATAL_ERROR
      "Fortran OpenMP lowering semantic prerequisites are missing:\n"
      "  - ${_omp_fortran_missing_msg}")
  endif()
endif()

function(add_omp_fortran_semantic_test specimen)
  _omp_sanitize_name(_omp_name ${specimen})
  set(_test_name omp_lowering_fortran_semantic_${_omp_name})
  set(_workdir ${_omp_fortran_lowering_output_dir}/semantic/${_omp_name})
  # Total timeout = compile/translation overhead + timed semantic runs.
  math(EXPR _total_timeout
       "${OMP_FORTRAN_SEMANTIC_COMPILE_OVERHEAD_SECONDS} + ${OMP_FORTRAN_SEMANTIC_TOTAL_RUNS} * ${OMP_FORTRAN_SEMANTIC_RUN_TIMEOUT_SECONDS}")
  add_test(
    NAME ${_test_name}
    WORKING_DIRECTORY ${_omp_fortran_lowering_output_dir}
    COMMAND ${BASH_EXECUTABLE} ${OMP_LOWERING_FORTRAN_SEMANTIC_SCRIPT}
            $<TARGET_FILE:roseomp>
            ${_omp_fortran_compiler}
            ${_omp_fortran_dir}/${specimen}
            ${_workdir}
            sort
            ${_omp_fortran_omp_include}
            ${ROSE_LLVM_OPENMP_RUNTIME_DIR}
            ${OMP_LOWERING_INC_DIR}
            $<TARGET_FILE:kmpc_fortran_abi>
            ${specimen}
            ${OMP_FORTRAN_SEMANTIC_RUN_TIMEOUT_SECONDS})
  _omp_set_test_timeout(${_test_name} ${_total_timeout})
  if(_omp_fortran_sanitizer_test_environment)
    set_property(TEST ${_test_name} APPEND PROPERTY ENVIRONMENT
                 "${_omp_fortran_sanitizer_test_environment}")
  endif()
  set_property(TEST ${_test_name} APPEND PROPERTY LABELS
               "${OMP_LOWERING_FORTRAN_LABELS};OMPLOWERING_SEMANTIC;OMPLOWERING_FORTRAN")
endfunction()

function(add_omp_fortran_invariant_test specimen dependency_test_name workdir)
  _omp_sanitize_name(_omp_name ${specimen})
  set(_inv_name omp_lowering_fortran_invariant_${_omp_name})
  add_test(
    NAME ${_inv_name}
    WORKING_DIRECTORY ${_omp_fortran_lowering_output_dir}
    COMMAND ${BASH_EXECUTABLE} ${OMP_LOWERING_INVARIANT_VERIFY_SCRIPT}
            ${_omp_fortran_dir}/${specimen}
            ${workdir}
            ${specimen})
  set_tests_properties(${_inv_name}
    PROPERTIES DEPENDS ${dependency_test_name})
  _omp_set_test_timeout(${_inv_name} 360)
  set_property(TEST ${_inv_name} APPEND PROPERTY LABELS
               "${OMP_LOWERING_FORTRAN_LABELS};OMPLOWERING_INVARIANT;OMPLOWERING_FORTRAN")
endfunction()

function(add_omp_fortran_ast_json_checkpoint_test specimen checkpoint)
  _omp_sanitize_name(_omp_name ${specimen})
  set(_test_name ASTJSON_omp_lowering_fortran_${checkpoint}_${_omp_name})
  string(REPLACE "-" "_" _test_name ${_test_name})
  set(_workdir ${_omp_fortran_lowering_output_dir}/ast-json/${_omp_name})
  math(EXPR _total_timeout
       "2 * (${OMP_FORTRAN_SEMANTIC_COMPILE_OVERHEAD_SECONDS} + ${OMP_FORTRAN_SEMANTIC_TOTAL_RUNS} * ${OMP_FORTRAN_SEMANTIC_RUN_TIMEOUT_SECONDS})")
  add_test(
    NAME ${_test_name}
    WORKING_DIRECTORY ${_omp_fortran_lowering_output_dir}
    COMMAND ${BASH_EXECUTABLE} ${OMP_LOWERING_FORTRAN_CHECKPOINT_SCRIPT}
            ${OMP_LOWERING_FORTRAN_SEMANTIC_SCRIPT}
            $<TARGET_FILE:roseomp>
            ${_omp_fortran_compiler}
            ${_omp_fortran_dir}/${specimen}
            ${_workdir}
            sort
            ${_omp_fortran_omp_include}
            ${ROSE_LLVM_OPENMP_RUNTIME_DIR}
            ${OMP_LOWERING_INC_DIR}
            $<TARGET_FILE:kmpc_fortran_abi>
            ${specimen}
            ${OMP_FORTRAN_SEMANTIC_RUN_TIMEOUT_SECONDS}
            ${checkpoint})
  _omp_set_test_timeout(${_test_name} ${_total_timeout})
  if(_omp_fortran_sanitizer_test_environment)
    set_property(TEST ${_test_name} APPEND PROPERTY ENVIRONMENT
                 "${_omp_fortran_sanitizer_test_environment}")
  endif()
  set_property(TEST ${_test_name} APPEND PROPERTY LABELS
               "ASTJSON;${OMP_LOWERING_FORTRAN_LABELS};OMPLOWERING_SEMANTIC;OMPLOWERING_FORTRAN")
endfunction()

if(enable-fortran)
  foreach(specimen IN LISTS OMP_FORTRAN_ALL_CASES)
    _omp_sanitize_name(_omp_name ${specimen})
    add_omp_fortran_semantic_test(${specimen})
    add_omp_fortran_invariant_test(
      ${specimen}
      omp_lowering_fortran_semantic_${_omp_name}
      ${_omp_fortran_lowering_output_dir}/semantic/${_omp_name})
  endforeach()

  add_omp_fortran_ast_json_checkpoint_test(schedule.f all)
  add_omp_fortran_ast_json_checkpoint_test(exampleA41f.f90 all)
endif()
