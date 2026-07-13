set(_omp_fortran_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/OpenMP_tests/fortran)

file(GLOB OMP_FORTRAN_ALL_CASES RELATIVE ${_omp_fortran_dir}
  ${_omp_fortran_dir}/*.f
  ${_omp_fortran_dir}/*.f90)
list(SORT OMP_FORTRAN_ALL_CASES)

# These specimens are required to fail in the Flang frontend and have exact
# diagnostic tests in CompileTests/OpenMP_tests/fortran.  They cannot enter a
# lowering semantic comparison because no valid original executable exists.
list(REMOVE_ITEM OMP_FORTRAN_ALL_CASES
  allocate_predefined_modifiers.f90
  allocate_user_defined_modifiers.f90
  exampleA134f.f90
  single-copyprivate.f90)

# This is a frontend semantic-identity fixture for DECLARE SIMD and DECLARE
# VARIANT.  Its dedicated typed-AST test below CompileTests/OpenMP_tests/fortran
# owns that contract; REX lowering rejects those unsupported declarative
# transformation directives before changing the AST.
list(REMOVE_ITEM OMP_FORTRAN_ALL_CASES
  rex_fortran_openmp_exact_semantic_handoff.f90)

# These are unparser-only lexical fixtures for continued OpenMP directives.
# Flang 22 cannot generate code for OpenMPUtilityConstruct, and REX does not
# implement Fortran target offload.  Dedicated continuation-contract tests in
# CompileTests/OpenMP_tests/fortran check their complete parse/unparse paths and
# fail hard on any mismatch.
list(REMOVE_ITEM OMP_FORTRAN_ALL_CASES
  rex_fortran_openmp_directive_string_continuation.f90
  rex_fortran_openmp_fixed_directive_continuation.f
  rex_fortran_openmp_free_directive_continuation.f90)
set(_omp_fortran_lowering_output_dir
  ${CMAKE_CURRENT_BINARY_DIR}/test-output)
file(MAKE_DIRECTORY ${_omp_fortran_lowering_output_dir})

# Semantic runs execute lowered and original binaries multiple times and can
# legitimately run longer when the full OMP lowering suite is parallelized.
# Both the original and lowered source must compile and execute successfully.
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
  set(_oracle_source ${_omp_fortran_dir}/${specimen})
  if(specimen STREQUAL "requires_dynamic_allocators_type.f90")
    set(_oracle_source
      ${CMAKE_CURRENT_SOURCE_DIR}/oracles/rex_fortran_openmp_requires_dynamic_allocators_type_oracle.f90)
  elseif(specimen STREQUAL "rex_fortran_openmp_allocate_predefined_semantic_positive.f90")
    set(_oracle_source
      ${CMAKE_CURRENT_SOURCE_DIR}/oracles/rex_fortran_openmp_allocate_predefined_oracle.f90)
  elseif(specimen STREQUAL "rex_fortran_openmp_allocate_user_semantic_positive.f90")
    set(_oracle_source
      ${CMAKE_CURRENT_SOURCE_DIR}/oracles/rex_fortran_openmp_allocate_user_oracle.f90)
  endif()
  if(NOT EXISTS "${_oracle_source}")
    message(FATAL_ERROR
      "Fortran OpenMP semantic oracle does not exist: ${_oracle_source}")
  endif()
  add_test(
    NAME ${_test_name}
    WORKING_DIRECTORY ${_omp_fortran_lowering_output_dir}
    COMMAND ${BASH_EXECUTABLE} ${OMP_LOWERING_FORTRAN_SEMANTIC_SCRIPT}
            $<TARGET_FILE:roseomp>
            ${_omp_fortran_compiler}
            ${_omp_fortran_dir}/${specimen}
            ${_oracle_source}
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

  add_test(
    NAME omp_lowering_fortran_negative_unsupported_declarative
    WORKING_DIRECTORY ${_omp_fortran_lowering_output_dir}
    COMMAND ${BASH_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/run_fortran_lowering_negative.sh
            $<TARGET_FILE:roseomp>
            ${_omp_fortran_dir}/rex_fortran_openmp_exact_semantic_handoff.f90
            ${_omp_fortran_lowering_output_dir}/negative/unsupported-declarative
            ${_omp_fortran_omp_include}
            REX_OMP_LOWERING_INVARIANT[unsupported-declarative])
  _omp_set_test_timeout(
    omp_lowering_fortran_negative_unsupported_declarative 360)
  set_property(
    TEST omp_lowering_fortran_negative_unsupported_declarative
    APPEND PROPERTY LABELS
    "${OMP_LOWERING_FORTRAN_LABELS};OMPLOWERING_NEGATIVE;OMPLOWERING_FORTRAN")
endif()
