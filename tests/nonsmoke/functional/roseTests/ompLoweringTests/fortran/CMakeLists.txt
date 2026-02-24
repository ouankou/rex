set(_omp_fortran_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/OpenMP_tests/fortran)

include(${_omp_fortran_dir}/OpenMP_fortran_Testcodes.cmake)

set(OMP_FORTRAN_LOWERING_EXCLUDED_CASES
  requires_dynamic_allocators_type.f90)

set(OMP_FORTRAN_ALL_CASES ${OPENMP_FORTRAN_TESTCODES})
list(REMOVE_ITEM OMP_FORTRAN_ALL_CASES
     ${OMP_FORTRAN_LOWERING_EXCLUDED_CASES})

# These specimens are syntax/AST-oriented and are not valid, deterministic
# runtime semantic checks under the LLVM Flang/OpenMP runtime toolchain.
set(OMP_FORTRAN_SEMANTIC_EXCLUDED_CASES
  allocate_predefined_modifiers.f90
  allocate_user_defined_modifiers.f90
  exampleA134f.f90
  single-copyprivate.f90)

set(OMP_FORTRAN_SEMANTIC_CASES ${OMP_FORTRAN_ALL_CASES})
list(REMOVE_ITEM OMP_FORTRAN_SEMANTIC_CASES
     ${OMP_FORTRAN_SEMANTIC_EXCLUDED_CASES})

set(OMP_FORTRAN_INVARIANT_EXCLUDED_CASES)

set(OMP_FORTRAN_INVARIANT_CASES ${OMP_FORTRAN_SEMANTIC_CASES})
list(REMOVE_ITEM OMP_FORTRAN_INVARIANT_CASES
     ${OMP_FORTRAN_INVARIANT_EXCLUDED_CASES})

# Translate-only specimens that intentionally do not enforce invariants.
set(OMP_FORTRAN_TRANSLATE_INVARIANT_EXCLUDED_CASES
  exampleA134f.f90)

list(REMOVE_DUPLICATES OMP_FORTRAN_ALL_CASES)
list(REMOVE_DUPLICATES OMP_FORTRAN_LOWERING_EXCLUDED_CASES)
list(REMOVE_DUPLICATES OMP_FORTRAN_SEMANTIC_CASES)
list(REMOVE_DUPLICATES OMP_FORTRAN_SEMANTIC_EXCLUDED_CASES)
list(REMOVE_DUPLICATES OMP_FORTRAN_INVARIANT_CASES)
list(REMOVE_DUPLICATES OMP_FORTRAN_INVARIANT_EXCLUDED_CASES)
list(REMOVE_DUPLICATES OMP_FORTRAN_TRANSLATE_INVARIANT_EXCLUDED_CASES)

foreach(specimen IN LISTS OMP_FORTRAN_LOWERING_EXCLUDED_CASES)
  if(NOT specimen IN_LIST OPENMP_FORTRAN_TESTCODES)
    message(FATAL_ERROR
      "Fortran lowering exclusion '${specimen}' is not in OPENMP_FORTRAN_TESTCODES")
  endif()
endforeach()

foreach(specimen IN LISTS OMP_FORTRAN_SEMANTIC_CASES)
  if(NOT specimen IN_LIST OMP_FORTRAN_ALL_CASES)
    message(FATAL_ERROR
      "Fortran semantic case '${specimen}' is not in OMP_FORTRAN_ALL_CASES")
  endif()
endforeach()

foreach(specimen IN LISTS OMP_FORTRAN_SEMANTIC_EXCLUDED_CASES)
  if(NOT specimen IN_LIST OMP_FORTRAN_ALL_CASES)
    message(FATAL_ERROR
      "Fortran semantic exclusion '${specimen}' is not in OMP_FORTRAN_ALL_CASES")
  endif()
endforeach()

foreach(specimen IN LISTS OMP_FORTRAN_INVARIANT_CASES)
  if(NOT specimen IN_LIST OMP_FORTRAN_SEMANTIC_CASES)
    message(FATAL_ERROR
      "Fortran invariant case '${specimen}' is not in OMP_FORTRAN_SEMANTIC_CASES")
  endif()
endforeach()

foreach(specimen IN LISTS OMP_FORTRAN_INVARIANT_EXCLUDED_CASES)
  if(NOT specimen IN_LIST OMP_FORTRAN_SEMANTIC_CASES)
    message(FATAL_ERROR
      "Fortran invariant exclusion '${specimen}' is not in OMP_FORTRAN_SEMANTIC_CASES")
  endif()
endforeach()

foreach(specimen IN LISTS OMP_FORTRAN_TRANSLATE_INVARIANT_EXCLUDED_CASES)
  if(NOT specimen IN_LIST OMP_FORTRAN_SEMANTIC_EXCLUDED_CASES)
    message(FATAL_ERROR
      "Fortran translate-invariant exclusion '${specimen}' is not in OMP_FORTRAN_SEMANTIC_EXCLUDED_CASES")
  endif()
endforeach()

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

function(add_omp_fortran_translate_test specimen)
  _omp_sanitize_name(_omp_name ${specimen})
  set(_test_name omp_lowering_fortran_translate_${_omp_name})
  set(_workdir ${CMAKE_CURRENT_BINARY_DIR}/translate/${_omp_name})
  set(_translator_flags)
  if(_omp_fortran_omp_include)
    list(APPEND _translator_flags -I${_omp_fortran_omp_include})
  endif()
  add_test(
    NAME ${_test_name}
    COMMAND ${BASH_EXECUTABLE} ${OMP_LOWERING_TRANSLATE_SCRIPT}
            $<TARGET_FILE:roseomp>
            ${_omp_fortran_dir}/${specimen}
            ${_workdir}
            ${specimen}
            ${_translator_flags})
  set_tests_properties(${_test_name} PROPERTIES TIMEOUT 540)
  set_property(TEST ${_test_name} APPEND PROPERTY LABELS
               "${OMP_LOWERING_FORTRAN_LABELS};OMPLOWERING_TRANSLATE;OMPLOWERING_FORTRAN")
endfunction()

function(add_omp_fortran_semantic_test specimen)
  _omp_sanitize_name(_omp_name ${specimen})
  set(_test_name omp_lowering_fortran_semantic_${_omp_name})
  set(_workdir ${CMAKE_CURRENT_BINARY_DIR}/semantic/${_omp_name})
  add_test(
    NAME ${_test_name}
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
            60)
  set_tests_properties(${_test_name} PROPERTIES TIMEOUT 1080)
  set_property(TEST ${_test_name} APPEND PROPERTY LABELS
               "${OMP_LOWERING_FORTRAN_LABELS};OMPLOWERING_SEMANTIC;OMPLOWERING_FORTRAN")
endfunction()

function(add_omp_fortran_invariant_test specimen dependency_test_name workdir)
  _omp_sanitize_name(_omp_name ${specimen})
  set(_inv_name omp_lowering_fortran_invariant_${_omp_name})
  add_test(
    NAME ${_inv_name}
    COMMAND ${BASH_EXECUTABLE} ${OMP_LOWERING_INVARIANT_VERIFY_SCRIPT}
            ${_omp_fortran_dir}/${specimen}
            ${workdir}
            ${specimen})
  set_tests_properties(${_inv_name}
    PROPERTIES DEPENDS ${dependency_test_name} TIMEOUT 360)
  set_property(TEST ${_inv_name} APPEND PROPERTY LABELS
               "${OMP_LOWERING_FORTRAN_LABELS};OMPLOWERING_INVARIANT;OMPLOWERING_FORTRAN")
endfunction()

if(enable-fortran)
  foreach(specimen IN LISTS OMP_FORTRAN_ALL_CASES)
    _omp_sanitize_name(_omp_name ${specimen})
    if(specimen IN_LIST OMP_FORTRAN_SEMANTIC_CASES)
      add_omp_fortran_semantic_test(${specimen})
      if(specimen IN_LIST OMP_FORTRAN_INVARIANT_CASES)
        add_omp_fortran_invariant_test(
          ${specimen}
          omp_lowering_fortran_semantic_${_omp_name}
          ${CMAKE_CURRENT_BINARY_DIR}/semantic/${_omp_name})
      endif()
    else()
      add_omp_fortran_translate_test(${specimen})
      if(NOT specimen IN_LIST OMP_FORTRAN_TRANSLATE_INVARIANT_EXCLUDED_CASES)
        add_omp_fortran_invariant_test(
          ${specimen}
          omp_lowering_fortran_translate_${_omp_name}
          ${CMAKE_CURRENT_BINARY_DIR}/translate/${_omp_name})
      endif()
    endif()
  endforeach()
endif()
