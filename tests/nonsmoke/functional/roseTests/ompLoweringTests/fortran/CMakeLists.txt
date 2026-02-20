set(_omp_fortran_dir
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/OpenMP_tests/fortran)

set(OMP_FORTRAN_ALL_CASES
  array-one.f
  array_initialize.f
  barrier.f
  conditional.f
  helloworld.f
  helloworld0.f
  helloworld2.f
  master.f
  jacobi.f
  ompdo-decremental.f
  ompdo-default.f
  ompdo-multiple.f
  ompdo-orphaned.f
  ompdo-static.f
  parallel-if.f
  parallel-if-numthreads.f
  parallel-1.f
  parallel-2.f
  parallel.f
  private.f
  reduction.f
  section1.f
  shared-array.f
  shared.f
  single.f
  task_largenumber.f
  task_scalar.f
  task_simplest.f)

set(OMP_FORTRAN_SEMANTIC_CASES
  barrier.f
  conditional.f
  helloworld.f
  helloworld0.f
  helloworld2.f
  parallel-1.f
  parallel-2.f
  parallel.f)

list(REMOVE_DUPLICATES OMP_FORTRAN_ALL_CASES)
list(REMOVE_DUPLICATES OMP_FORTRAN_SEMANTIC_CASES)

foreach(specimen IN LISTS OMP_FORTRAN_SEMANTIC_CASES)
  if(NOT specimen IN_LIST OMP_FORTRAN_ALL_CASES)
    message(FATAL_ERROR "Fortran semantic case '${specimen}' is not in OMP_FORTRAN_ALL_CASES")
  endif()
endforeach()

set(_omp_fortran_semantic_enabled ON)
set(_omp_fortran_compiler "")
set(_omp_fortran_omp_include "")

if(NOT enable-fortran)
  set(_omp_fortran_semantic_enabled OFF)
endif()

if(_omp_fortran_semantic_enabled)
  if(CMAKE_Fortran_COMPILER)
    set(_omp_fortran_compiler ${CMAKE_Fortran_COMPILER})
  else()
    find_program(_omp_fortran_compiler
      NAMES flang-${CLANG_VERSION_MAJOR} flang
      HINTS ${LLVM_TOOLS_BINARY_DIR})
  endif()
  if(NOT _omp_fortran_compiler)
    message(WARNING "Flang not found; using translate-only mode for Fortran lowering tests")
    set(_omp_fortran_semantic_enabled OFF)
  endif()
endif()

if(_omp_fortran_semantic_enabled)
  if(ROSE_LLVM_OPENMP_RUNTIME_DIR)
    get_filename_component(_llvm_runtime_prefix
      "${ROSE_LLVM_OPENMP_RUNTIME_DIR}/.." ABSOLUTE)
    if(EXISTS "${_llvm_runtime_prefix}/include/flang/omp_lib.h")
      set(_omp_fortran_omp_include "${_llvm_runtime_prefix}/include/flang")
    endif()
  endif()
  if(NOT _omp_fortran_omp_include)
    if(_omp_fortran_compiler)
      get_filename_component(_flang_bin_dir "${_omp_fortran_compiler}" DIRECTORY)
      get_filename_component(_flang_prefix "${_flang_bin_dir}/.." ABSOLUTE)
      if(EXISTS "${_flang_prefix}/include/flang/omp_lib.h")
        set(_omp_fortran_omp_include "${_flang_prefix}/include/flang")
      endif()
    endif()
  endif()
  if(NOT _omp_fortran_omp_include)
    message(WARNING "LLVM Fortran OpenMP include directory not found; using translate-only mode for Fortran lowering tests")
    set(_omp_fortran_semantic_enabled OFF)
  endif()
endif()

if(_omp_fortran_semantic_enabled)
  if(NOT ROSE_LLVM_OPENMP_RUNTIME_DIR)
    message(WARNING "LLVM OpenMP runtime directory unavailable; using translate-only mode for Fortran lowering tests")
    set(_omp_fortran_semantic_enabled OFF)
  endif()
endif()

if(_omp_fortran_semantic_enabled)
  if(NOT TARGET kmpc_fortran_abi)
    message(WARNING "kmpc_fortran_abi target unavailable; using translate-only mode for Fortran lowering tests")
    set(_omp_fortran_semantic_enabled OFF)
  endif()
endif()

function(add_omp_fortran_translate_test specimen)
  _omp_sanitize_name(_omp_name ${specimen})
  set(_test_name omp_lowering_fortran_compile_only_${_omp_name})
  set(_workdir ${CMAKE_CURRENT_BINARY_DIR}/compile_only/${_omp_name})
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
  set_tests_properties(${_test_name} PROPERTIES TIMEOUT 180)
  set_property(TEST ${_test_name} APPEND PROPERTY LABELS
               "${OMP_LOWERING_FORTRAN_LABELS};OMPLOWERING_LEGACY_TRANSLATE;OMPLOWERING_COMPILE_ONLY;OMPLOWERING_FORTRAN")
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
  set_tests_properties(${_test_name} PROPERTIES TIMEOUT 360)
  set_property(TEST ${_test_name} APPEND PROPERTY LABELS
               "${OMP_LOWERING_FORTRAN_LABELS};OMPLOWERING_SEMANTIC;OMPLOWERING_FORTRAN")

  set(_inv_name omp_lowering_fortran_invariant_${_omp_name})
  add_test(
    NAME ${_inv_name}
    COMMAND ${BASH_EXECUTABLE} ${OMP_LOWERING_INVARIANT_VERIFY_SCRIPT}
            ${_omp_fortran_dir}/${specimen}
            ${_workdir}
            ${specimen})
  set_tests_properties(${_inv_name} PROPERTIES DEPENDS ${_test_name} TIMEOUT 120)
  set_property(TEST ${_inv_name} APPEND PROPERTY LABELS
               "${OMP_LOWERING_FORTRAN_LABELS};OMPLOWERING_INVARIANT;OMPLOWERING_FORTRAN")
endfunction()

if(enable-fortran)
  foreach(specimen IN LISTS OMP_FORTRAN_ALL_CASES)
    if(specimen IN_LIST OMP_FORTRAN_SEMANTIC_CASES AND _omp_fortran_semantic_enabled)
      add_omp_fortran_semantic_test(${specimen})
    else()
      add_omp_fortran_translate_test(${specimen})
    endif()
  endforeach()
endif()
