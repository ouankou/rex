include_directories(${ROSE_INCLUDES} ${CMAKE_CURRENT_BINARY_DIR})

find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; SageInterface death tests require bash")
endif()

file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/test_output_path.h CONTENT [=[
#pragma once

#include <string>

#include "rose_test_output_path.h"

inline std::string resolveTestOutputPath(const std::string &filename) {
  return Rose::TestOutput::resolvePath(filename);
}
]=])

add_executable(getDataSharingAttribute getDataSharingAttribute.C)
target_link_libraries(getDataSharingAttribute ROSE_DLL ${link_with_libraries})

add_executable(get_C_array_dimensions get_C_array_dimensions.C)
target_link_libraries(get_C_array_dimensions ROSE_DLL ${link_with_libraries})

add_executable(genericCoverage genericCoverage.C)
target_link_libraries(genericCoverage ROSE_DLL ${link_with_libraries})

add_executable(collectFunctionSideEffect collectFunctionSideEffect.C)
target_link_libraries(collectFunctionSideEffect ROSE_DLL ${link_with_libraries})

add_executable(performAliasAnalysis performAliasAnalysis.C)
target_link_libraries(performAliasAnalysis ROSE_DLL ${link_with_libraries})

add_executable(rex_integer_width_contract rex_integer_width_contract.cpp)
target_include_directories(rex_integer_width_contract PRIVATE
  ${ROSE_TOP_SRC_DIR}/src/frontend/SageIII/sageInterface)
add_test(NAME rex_integer_width_contract COMMAND rex_integer_width_contract)
set_tests_properties(rex_integer_width_contract
  PROPERTIES LABELS "rex;integer-width;WASM")

set(_openmp_modifier_validation_header_test
  rex_openmp_modifier_validation_header_self_contained.cpp)
add_executable(rex_openmp_modifier_validation_header_self_contained
  ${_openmp_modifier_validation_header_test})
set_source_files_properties(${_openmp_modifier_validation_header_test}
  PROPERTIES SKIP_PRECOMPILE_HEADERS TRUE)
target_link_libraries(rex_openmp_modifier_validation_header_self_contained
  ROSE_DLL ${link_with_libraries})
add_test(
  NAME rex_openmp_modifier_validation_header_self_contained
  COMMAND rex_openmp_modifier_validation_header_self_contained)
set_tests_properties(rex_openmp_modifier_validation_header_self_contained
  PROPERTIES LABELS "rex;headers;OpenMP;WASM")

add_test(
  NAME rex_omp_support_header_self_contained
  COMMAND ${CMAKE_CXX_COMPILER}
    -std=c++17
    -fsyntax-only
    -I${ROSE_TOP_SRC_DIR}/src/frontend/SageIII
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_omp_support_header_self_contained.cpp)
set_tests_properties(rex_omp_support_header_self_contained
  PROPERTIES LABELS "rex;headers;CFE")

add_test(
  NAME rex_build_script_preserves_search_paths
  COMMAND ${BASH_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_build_script_preserves_search_paths.sh
    ${ROSE_TOP_SRC_DIR}/build-rex.sh)
set_tests_properties(rex_build_script_preserves_search_paths
  PROPERTIES LABELS "rex;build-system")

add_test(
  NAME rex_ci_llvm_env_preserves_search_paths
  COMMAND ${BASH_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/rex_ci_llvm_env_preserves_search_paths.sh
    ${ROSE_TOP_SRC_DIR}/scripts/ci-llvm-env)
set_tests_properties(rex_ci_llvm_env_preserves_search_paths
  PROPERTIES LABELS "rex;build-system;CI")

set(_openmp_specimens_path
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/OpenMP_tests)
set(_dataracebench_path ${_openmp_specimens_path}/dataRaceBench)
set(_c_specimens_path
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/C_tests)
set(_cxx_specimens_path
  ${CMAKE_SOURCE_DIR}/tests/nonsmoke/functional/CompileTests/Cxx_tests)
set(_alias_specimens_path ${CMAKE_CURRENT_SOURCE_DIR}/tests)
set(_reference_dir ${CMAKE_CURRENT_SOURCE_DIR}/referenceOutput)
set(_sage_output_dir ${CMAKE_CURRENT_BINARY_DIR}/test-output)
file(MAKE_DIRECTORY ${_sage_output_dir})
set(_test_flags)
#Most C DataRaceBench specimens rely on GNU89 implicit - declaration semantics
#during parsing, but a few use C99 - only keywords such as `restrict`.
set(_datarace_c_gnu89_test_flags -std=gnu89)
set(_datarace_c_gnu99_test_flags -std=gnu99)

set(SAGE_OPENMP_SPECIMENS
  3loops.c
  alignment.c
  array_init_2.c
  array_init.c
  atomic.c
  atoms-2.c
  rex_openmp_axpy_multi_device.c
  axpy_ompacc3.c
  axpy_ompacc.c
  barrier.c
  bonds-2.c
  collapse_2.c
  collapse.c
  copyin.c
  copyprivate2.c
  copyprivate3.c
  copyprivate.c
  critical.c
  critical_dead.c
  critical_orphaned.c
  dijkstra_open_mp.c
  dynamicChunk.c
  empty.c
  endif2.c
  endif3.c
  endif.c
  expressions.c
  falsesharing.c
  firstlastprivate.c
  firstPrivateArray_2.c
  firstPrivateArray.c
  firstprivate.c
  flush.c
  flush_exampleA_21_1c.c
  full_verify.c
  get_max_threads.c
  hello-1.c
  hello-2.c
  hello.c
  hello-ordered.c
  include_header_only.c
  init.c
  jacobi.c
  jacobi-collapse.c
  jacobi-ompacc.c
  jacobi-ompacc-multiGPU.c
  jacobi-ompacc-opt1.c
  jacobi-ompacc-opt2.c
  jacobi-ompacc-v2.c
  lastprivate0.c
  lastprivate.c
  lastprivateOrphaned.c
  limits_threads.c
  linebreak.c
  lockarray.c
  loop1.c
  lu_factorization.c
  macroIds.c
  master.c
  masterSingle.c
  matrixmultiply.c
  matrixmultiply-ompacc2.c
  matrixmultiply-ompacc.c
  matrix_vector.c
  md_open_mp.c
  multiple_return.c
  nestedpar1.c
  nestedpar.c
  nowait.c
  omp1.c
  ompfor2.c
  ompfor3.c
  ompfor4.c
  ompfor5.c
  ompfor6.c
  ompfor7.c
  ompfor.c
  ompfor_c99.c
  ompfor-decremental.c
  ompfor-default.c
  ompfor-static.c
  ompGetNumThreads.c
  omp_sections.c
  ordered2.c
  ordered.c
  orphaned-directives.c
  parallel.c
  parallelfor2.c
  parallelfor.c
  parallel-if.c
  parallel-if-numthreads.c
  parallel-numthreads.c
  parallel-reduction2.c
  parallel-reduction.c
  parallelsections.c
  preprocessingInfo.c
  prettyfunc.c
  private.c
  private-duplicate.c
  privatej.c
  recursive.c
  reduction2.c
  reduction.c
  reduction-classic.c
  reduction_min_max.c
  rice1.c
  section1.c
  section.c
  set_num_threads.c
  shared.c
  single2.c
  single.c
  single_copyprivate.c
  sizeof.c
  spmd1.c
  staticChunk.c
  subteam2.c
  subteam.c
  task_array2.c
  task_array.c
  task_largenumber.c
  task_orphaned.c
  task_outlining.c
  task_underIf.c
  task_untied2.c
  task_untied3.c
  task_untied4.c
  task_untied.c
  task_wait2.c
  task_wait.c
  threadprivate2.c
  threadprivate3.c
  threadprivate.c
  threadProcessor.c
  twoRegions.c
  upperCase.c
  variables.c
)

set(SAGE_OPENMP_VERIFY
  3loops.c
  alignment.c
  array_init_2.c
  array_init.c
  atomic.c
  atoms-2.c
  rex_openmp_axpy_multi_device.c
  axpy_ompacc3.c
  axpy_ompacc.c
  barrier.c
  bonds-2.c
  collapse_2.c
  collapse.c
  copyin.c
  copyprivate2.c
  copyprivate3.c
  copyprivate.c
  critical.c
  critical_dead.c
  critical_orphaned.c
  dijkstra_open_mp.c
  dynamicChunk.c
  empty.c
  endif2.c
  endif3.c
  endif.c
  reduction_min_max.c
)

set(SAGE_DATARACE_C
  3mm-tile-no.c
  adi-tile-no.c
  3mm-parallel-no.c
  adi-parallel-no.c
  antidep1-orig-yes.c
  antidep1-var-yes.c
  antidep2-orig-yes.c
  antidep2-var-yes.c
  doall1-orig-no.c
  doall2-orig-no.c
  doall2-orig-yes.c
  doallchar-orig-no.c
  firstprivate-orig-no.c
  flush-orig-no.c
  flush-orig-yes.c
  fprintf-orig-no.c
  functionparameter-orig-no.c
  getthreadnum-orig-no.c
  getthreadnum-orig-yes.c
  indirectaccess1-orig-yes.c
  indirectaccess2-orig-yes.c
  indirectaccess3-orig-yes.c
  indirectaccess4-orig-yes.c
  indirectaccesssharebase-orig-no.c
  inneronly1-orig-no.c
  inneronly2-orig-no.c
  jacobi2d-parallel-no.c
  jacobi2d-tile-no.c
  jacobiinitialize-orig-no.c
  jacobikernel-orig-no.c
  lastprivatemissing-orig-yes.c
  lastprivatemissing-var-yes.c
  lastprivate-orig-no.c
  matrixmultiply-orig-no.c
  matrixvector1-orig-no.c
  matrixvector2-orig-no.c
  minusminus-orig-yes.c
  minusminus-var-yes.c
  nowait-orig-yes.c
  outeronly1-orig-no.c
  outeronly2-orig-no.c
  outofbounds-orig-yes.c
  outofbounds-var-yes.c
  outputdep-orig-yes.c
  outputdep-var-yes.c
  pireduction-orig-no.c
  plusplus-orig-yes.c
  plusplus-var-yes.c
  pointernoaliasing-orig-no.c
  privatemissing-orig-yes.c
  privatemissing-var-yes.c
  reductionmissing-orig-yes.c
  reductionmissing-var-yes.c
  restrictpointer1-orig-no.c
  restrictpointer2-orig-no.c
  sections1-orig-yes.c
  sectionslock1-orig-no.c
  simd1-orig-no.c
  simdtruedep-orig-yes.c
  simdtruedep-var-yes.c
  single-orig-no.c
  targetparallelfor-orig-no.c
  targetparallelfor-orig-yes.c
  taskdep1-orig-no.c
  taskdep2-orig-no.c
  taskdep3-orig-no.c
  taskdependmissing-orig-yes.c
  truedep1-orig-yes.c
  truedep1-var-yes.c
  truedepfirstdimension-orig-yes.c
  truedepfirstdimension-var-yes.c
  truedeplinear-orig-yes.c
  truedeplinear-var-yes.c
  truedepscalar-orig-yes.c
  truedepscalar-var-yes.c
  truedepseconddimension-orig-yes.c
  truedepseconddimension-var-yes.c
  truedepsingleelement-orig-yes.c
  truedepsingleelement-var-yes.c
  2018-simd2-orig-no.c
  2018-parallel-for-simd-orig-no.c
  2018-threadprivate-orig-no.c
  2018-private-origin-no.c
  2018-static-origin-yes.c
  2018-collapse-orig-no.c
  2018-collapsemissing-orig-yes.c
  2018-ordered-orig-no.c
  2018-ordered-orig-yes.c
)

set(SAGE_DATARACE_CPP
  2018-static-data-member-orig-yes.cpp
)

set(SAGE_DATARACE_C_GNU99
  restrictpointer1-orig-no.c
  restrictpointer2-orig-no.c
)

set(SAGE_C_ARRAY_DIMENSIONS
  test2017_01_array.c
)

set(SAGE_GENERIC_COVERAGE
  helloworld.cpp
)

set(SAGE_COLLECT_FUNCTION_SIDE_EFFECT
  rex_readwriteset_address_of_cast.cpp
  rex_readwriteset_nested_memory_refs.cpp
  test2017_14_Liao.C
  test2017_15_Liao.C
)

set(SAGE_ALIAS_ANALYSIS
  alias-001.cpp
  alias-002.cpp
  alias-003.cpp
)

function(_sage_sanitize_name input output_var)
  set(_name "${input}")
  string(REPLACE "/" "_" _name "${_name}")
  string(REPLACE "." "_" _name "${_name}")
  string(REPLACE "-" "_" _name "${_name}")
  string(REPLACE "+" "_" _name "${_name}")
  set(${output_var} "${_name}" PARENT_SCOPE)
endfunction()

function(_sage_add_diff_test test_name output_file)
  if(ARGC GREATER 2)
    set(_reference_output ${ARGV2})
  else()
    set(_reference_output ${output_file})
  endif()
  add_test(
    NAME ${test_name}_diff
    COMMAND ${CMAKE_COMMAND} -E compare_files
      ${_sage_output_dir}/${output_file}
      ${_reference_dir}/${_reference_output}
  )
  set_tests_properties(${test_name}_diff PROPERTIES DEPENDS "${test_name}")
endfunction()

function(_sage_set_output_env test_name)
  set_property(TEST ${test_name} APPEND PROPERTY ENVIRONMENT
               "ROSE_TEST_OUTPUT_DIR=${_sage_output_dir}")
endfunction()

foreach(specimen IN LISTS SAGE_OPENMP_SPECIMENS)
  _sage_sanitize_name(${specimen} _name)
  set(_test_name sage_getDataSharingAttribute_${_name})
  set(_output ${specimen}.getDataSharingAttribute.output)
  add_test(
    NAME ${_test_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:getDataSharingAttribute>
      ${_test_flags}
      -c -rose:openmp:ast_only
      ${_openmp_specimens_path}/${specimen}
  )
  _sage_set_output_env(${_test_name})
  if(specimen IN_LIST SAGE_OPENMP_VERIFY)
    if(specimen STREQUAL "rex_openmp_axpy_multi_device.c")
      _sage_add_diff_test(${_test_name} ${_output}
        axpy_ompacc2.c.getDataSharingAttribute.output)
    else()
      _sage_add_diff_test(${_test_name} ${_output})
    endif()
  endif()
endforeach()

foreach(specimen IN LISTS SAGE_DATARACE_C)
  _sage_sanitize_name(${specimen} _name)
  set(_test_name sage_getDataSharingAttribute_datarace_${_name})
  set(_output ${specimen}.getDataSharingAttribute.output)
  set(_specimen_flags ${_datarace_c_gnu89_test_flags})
  if(specimen IN_LIST SAGE_DATARACE_C_GNU99)
    set(_specimen_flags ${_datarace_c_gnu99_test_flags})
  endif()
  add_test(
    NAME ${_test_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:getDataSharingAttribute>
      ${_test_flags}
      ${_specimen_flags}
      -I${_dataracebench_path}
      -c -rose:openmp:ast_only
      ${_dataracebench_path}/${specimen}
  )
  _sage_set_output_env(${_test_name})
  _sage_add_diff_test(${_test_name} ${_output})
endforeach()

foreach(specimen IN LISTS SAGE_DATARACE_CPP)
  _sage_sanitize_name(${specimen} _name)
  set(_test_name sage_getDataSharingAttribute_datarace_${_name})
  set(_output ${specimen}.getDataSharingAttribute.output)
  add_test(
    NAME ${_test_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:getDataSharingAttribute>
      ${_test_flags}
      -I${_dataracebench_path}
      -c -rose:openmp:ast_only
      ${_dataracebench_path}/${specimen}
  )
  _sage_set_output_env(${_test_name})
  _sage_add_diff_test(${_test_name} ${_output})
endforeach()

foreach(specimen IN LISTS SAGE_C_ARRAY_DIMENSIONS)
  _sage_sanitize_name(${specimen} _name)
  set(_test_name sage_get_C_array_dimensions_${_name})
  set(_output ${specimen}.get_C_array_dimensions.output)
  add_test(
    NAME ${_test_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:get_C_array_dimensions>
      ${_test_flags}
      -c ${_c_specimens_path}/${specimen}
  )
  _sage_set_output_env(${_test_name})
  _sage_add_diff_test(${_test_name} ${_output})
endforeach()

foreach(specimen IN LISTS SAGE_GENERIC_COVERAGE)
  _sage_sanitize_name(${specimen} _name)
  set(_test_name sage_genericCoverage_${_name})
  set(_output ${specimen}.genericCoverage.output)
  add_test(
    NAME ${_test_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:genericCoverage>
      ${_test_flags}
      -c ${_cxx_specimens_path}/${specimen}
  )
  _sage_set_output_env(${_test_name})
  _sage_add_diff_test(${_test_name} ${_output})
endforeach()

foreach(specimen IN LISTS SAGE_COLLECT_FUNCTION_SIDE_EFFECT)
  _sage_sanitize_name(${specimen} _name)
  set(_test_name sage_collectFunctionSideEffect_${_name})
  set(_output ${specimen}.collectFunctionSideEffect.output)
  add_test(
    NAME ${_test_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:collectFunctionSideEffect>
      -sort ${_test_flags}
      -annot ${CMAKE_CURRENT_SOURCE_DIR}/vectorXY.annot
      -c ${_cxx_specimens_path}/${specimen}
  )
  _sage_set_output_env(${_test_name})
  _sage_add_diff_test(${_test_name} ${_output})
endforeach()

foreach(specimen IN LISTS SAGE_ALIAS_ANALYSIS)
  _sage_sanitize_name(${specimen} _name)
  set(_test_name sage_performAliasAnalysis_${_name})
  set(_output ${specimen}.performAliasAnalysis.output)
  add_test(
    NAME ${_test_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND $<TARGET_FILE:performAliasAnalysis>
      ${_test_flags}
      -c ${_alias_specimens_path}/${specimen}
  )
  _sage_set_output_env(${_test_name})
  _sage_add_diff_test(${_test_name} ${_output})
endforeach()
