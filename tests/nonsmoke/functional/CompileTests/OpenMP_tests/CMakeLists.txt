add_executable(parseOmp parseOmp.C)
target_link_libraries(parseOmp ROSE_DLL ${link_with_libraries})

add_executable(checkOmpAnalyzing checkOmpAnalyzing.C)
target_link_libraries(checkOmpAnalyzing ROSE_DLL ${link_with_libraries})

find_program(BASH_EXECUTABLE bash)
if(NOT BASH_EXECUTABLE)
  message(FATAL_ERROR "bash not found; OpenMP_tests require bash")
endif()

if(ENABLE-C)
  add_subdirectory(cvalidation)
endif()

if(ENABLE-FORTRAN)
  add_subdirectory(fortran)
endif()

if(NOT ENABLE-C)
  return()
endif()

# C test codes
set(C_TESTCODES
	3loops.c affinity.c allocate.c jacobi.c jacobi-collapse.c alignment.c
	array_init.c array_init_2.c array_section_stride.c atoms-2.c atomic.c atomic-3.c barrier.c
	cancel.c cancellation_point.c collapse.c copyin.c copyprivate2.c copyprivate3.c
	copyprivate.c critical.c critical1.c critical_dead.c critical_orphaned.c
	depobj.c task_dep1.c task_dep3.c task_dep_iterator1.c task_dep_iterator2.c
	declare_mapper_target_update.c
	declare_simd.c distribute_parallel_for.c distribute_parallel_for_simd.c
	distribute_simd.c distribute.c distribute2.c dijkstra_open_mp.c dynamicChunk.c
	empty.c endif.c endif2.c endif3.c nowait.c expressions.c falsesharing.c
	firstprivate.c firstPrivateArray.c firstPrivateArray_2.c firstlastprivate.c
	flush.c flush_exampleA_21_1c.c full_verify.c flush_with_clauses.c
	get_max_threads.c hello.c hello-1.c hello-2.c hello-ordered.c
	include_header_only.c init.c lastprivate0.c lastprivate.c lastprivateOrphaned.c
	limits_threads.c linebreak.c lockarray.c loop.c loop1.c lu_factorization.c
	master.c masterSingle.c mastertaskloop.c mastertaskloopsimd.c metadirective_arch_nvidia.c matrix_vector.c
	matrixmultiply.c md_open_mp.c multiple_return.c nestedpar1.c nestedpar.c omp1.c
	ompfor.c ompfor_c99.c ompfor2.c ompfor3.c ompfor4.c ompfor5.c ompfor6.c
	ompfor8.c ompfor9.c ompfor10.c ompfor-default.c ompfor-decremental.c
	ompfor-static.c ompGetNumThreads.c omp_sections.c ordered3.c ordered2.c
	ordered.c ordered_depend.c orphaned-directives.c parallel.c
	parallel_allocate_predefined_modifier.c parallel_allocate_user_modifier.c
	parallel-if.c parallel-if-numthreads.c parallel_if_parallel.c
	parallel-numthreads.c parallel-reduction.c parallel-reduction2.c
	parallel_pragma.c parallelfor.c parallelfor2.c parallelsections.c
	parallelloop.c parallelmaster.c parallelmastertaskloop.c
	parallelmastertaskloopsimd.c pi.c preprocessingInfo.c prettyfunc.c private.c
	privatej.c private-duplicate.c recursive.c reduction2.c reduction.c
	reduction-classic.c reduction_min_max.c reduction_modifier.c
	reduction_user_identifier.c requires.c requires_atomic_default_mem_order.c
	requires_ext.c requires_reverse_offload.c requires_unified_address.c
	requires_dynamic_allocators.c requires_unified_shared_memory.c rice1.c scan.c
	section.c section1.c sections.c sections1.c set_num_threads.c shared.c
	simd2.c simd3.c simd4.c simd5.c single.c single2.c single3.c
	single_copyprivate.c sizeof.c spmd1.c staticChunk.c subteam2.c subteam.c
	target.c target1.c target_uses_allocators.c targetparallel.c
	targetparallelforsimd.c targetparallelloop.c targetsimd.c targetteams.c
	targetteamsdistribute.c targetteamsdistributesimd.c targetteamsloop.c
	targetteamsdistributeparallelfor.c targetteamsdistributeparallelforsimd.c
	targetupdate.c targetupdate_nowait.c targetupdate_device.c target_update_from.c
	target_literal_scalar.c
	target_update_to.c taskloop_simd_reduction.1.c task_affinity.c task_affinity1.c
	task_array.c task_array2.c task_final.c task_largenumber.c task_mergeable.c
	task_orphaned.c task_outlining.c task_priority.c task_untied.c task_untied2.c
	task_untied3.c task_untied4.c task_underIf.c taskgroup.c taskloop.c
	task_wait.c task_wait2.c taskwait.c taskwait2.c taskwait3.c taskwait4.c taskwait5.c
	taskyield.c
	threadprivate.c threadprivate2.c threadprivate3.c teams.c teamsdistribute.c
	teamsdistributesimd.c teamsdistributeparallelfor.c
	teamsdistributeparallelforsimd.c teamsloop.c twoRegions.c threadProcessor.c
	upperCase.c variables.c requires1.c simd.c ompfor7.c macroCalls.c
	define-directive.c while-task.c task_dep2.c task_dep.load.compute.c)

# C++ test codes
set(CXX_TESTCODES
	classMember.cpp hello1.cpp helloNested.cpp memberFunction.cpp
	memberFunction2.cpp objectPrivate.cpp objectFirstPrivate.cpp
	objectLastprivate.cpp referenceType.cpp declare_mapper_type_qualifiers.cpp
	orphanedAtomic.cpp preprocessingInfo2.cpp task_link.cpp task_link2.cpp
	task_tree.cpp jacobi-ompacc2.cpp data-member-access.cpp define-directive2.cpp)

set(TESTCODES ${C_TESTCODES} ${CXX_TESTCODES})

# OpenMP+OpenACC combined test codes
set(OMP_ACC_TESTCODES
  axpy_ompacc.c axpy_ompacc2.c axpy_ompacc3.c
  matrixmultiply-ompacc.c matrixmultiply-ompacc2.c
  jacobi-ompacc.c jacobi-ompacc-v2.c jacobi-ompacc-opt1.c jacobi-ompacc-opt2.c
  jacobi-ompacc-multiGPU.c axpy_ompacc_mpi.c axpy_ompacc4.c
  axpy_ompacc_parseonly.c)

# ROSE flags for OpenMP parsing (CFE frontend only).
# Do not force a C standard here; rely on the frontend default dialect.
set(ROSE_C_FLAGS -rose:openmp:ast_only -w -rose:verbose 0)
set(ROSE_CXX_FLAGS -rose:openmp:ast_only -w -rose:verbose 0)

set(_omp_test_output_dir ${CMAKE_CURRENT_BINARY_DIR}/test-output)
file(MAKE_DIRECTORY ${_omp_test_output_dir})

set(ROSE_OMP_ANALYZING_FLAGS -rose:openmp:analyzing -rose:skipfinalCompileStep -w -rose:verbose 0)

foreach(file_to_test ${TESTCODES})
  if(file_to_test MATCHES "\\.c$")
    set(_rose_flags ${ROSE_C_FLAGS})
  else()
    set(_rose_flags ${ROSE_CXX_FLAGS})
  endif()
  add_test(
    NAME OMPTEST_${file_to_test}
    WORKING_DIRECTORY ${_omp_test_output_dir}
    COMMAND parseOmp ${_rose_flags}
            -rose:output ${_omp_test_output_dir}/rose_${file_to_test}
            -c
            ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})
  set_tests_properties(OMPTEST_${file_to_test} PROPERTIES LABELS OMPTEST)
endforeach()

foreach(file_to_test ${OMP_ACC_TESTCODES})
  add_test(
    NAME OMPACCTEST_${file_to_test}
    WORKING_DIRECTORY ${_omp_test_output_dir}
    COMMAND parseOmp ${ROSE_C_FLAGS}
                     -rose:output ${_omp_test_output_dir}/rose_${file_to_test}
                     -rose:skipfinalCompileStep
                     -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})
  set_tests_properties(OMPACCTEST_${file_to_test} PROPERTIES LABELS OMPACCTEST)
endforeach()

# Multi-file test: verifies correct handling of multiple source files with OpenMP pragmas
add_test(
  NAME OMPTEST_bonds-2
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND parseOmp ${ROSE_C_FLAGS}
          -c ${CMAKE_CURRENT_SOURCE_DIR}/atoms-2.c
          ${CMAKE_CURRENT_SOURCE_DIR}/bonds-2.c)
set_tests_properties(OMPTEST_bonds-2 PROPERTIES LABELS OMPTEST)

# Test for macro ID handling with comment/directive collection
add_test(
  NAME OMPTEST_macroIds
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND parseOmp ${ROSE_C_FLAGS}
          -rose:output ${_omp_test_output_dir}/rose_macroIds.c
          -rose:collectAllCommentsAndDirectives
          ${CMAKE_CURRENT_SOURCE_DIR}/macroIds.c)
set_tests_properties(OMPTEST_macroIds PROPERTIES LABELS OMPTEST)

set(_omp_reference_dir ${CMAKE_CURRENT_SOURCE_DIR}/referenceResults)

function(add_omp_output_diff_tests input_file dependency label)
  string(REPLACE "." "_" _name "${input_file}")
  string(REPLACE "-" "_" _name "${_name}")
  set(_rose_file rose_${input_file})
  set(_output_file ${_rose_file}.output)
  set(_diff_file ${_output_file}.diff)

  add_test(
    NAME OMPTEST_output_${_name}
    WORKING_DIRECTORY ${_omp_test_output_dir}
    COMMAND ${BASH_EXECUTABLE} -c
      "grep -E \"^[[:space:]]*(#pragma[[:space:]]+omp|//[[:space:]]*#pragma[[:space:]]+omp|/\\*[[:space:]]*#pragma[[:space:]]+omp.*)\" ${_rose_file} > ${_output_file} || exit 0"
  )
  set_tests_properties(OMPTEST_output_${_name} PROPERTIES DEPENDS "${dependency}" LABELS "${label}")

  add_test(
    NAME OMPTEST_output_diff_${_name}
    WORKING_DIRECTORY ${_omp_test_output_dir}
    COMMAND ${BASH_EXECUTABLE} -c
      "if diff ${_output_file} ${_omp_reference_dir}/${_output_file} > ${_diff_file}; then rm -f ${_diff_file}; exit 0; else cat ${_diff_file}; rm -f ${_diff_file}; exit 1; fi"
  )
  set_tests_properties(OMPTEST_output_diff_${_name} PROPERTIES DEPENDS OMPTEST_output_${_name} LABELS "${label}")
endfunction()

foreach(file_to_test IN LISTS C_TESTCODES CXX_TESTCODES)
  add_omp_output_diff_tests(${file_to_test} OMPTEST_${file_to_test} OMPTEST)
endforeach()

foreach(file_to_test IN LISTS OMP_ACC_TESTCODES)
  add_omp_output_diff_tests(${file_to_test} OMPACCTEST_${file_to_test} OMPACCTEST)
endforeach()

add_omp_output_diff_tests(bonds-2.c OMPTEST_bonds-2 OMPTEST)

add_omp_output_diff_tests(macroIds.c OMPTEST_macroIds OMPTEST)

# Focused OpenMP analyzer regressions.
add_test(
  NAME OMPANALYZE_schedule_default_for
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND checkOmpAnalyzing
          --rex-check=schedule-default-for
          ${ROSE_OMP_ANALYZING_FLAGS}
          -c ${CMAKE_CURRENT_SOURCE_DIR}/ompfor-default.c)
set_tests_properties(OMPANALYZE_schedule_default_for PROPERTIES LABELS OMPANALYZE)

add_test(
  NAME OMPANALYZE_schedule_dynamic_for
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND checkOmpAnalyzing
          --rex-check=schedule-dynamic-for
          ${ROSE_OMP_ANALYZING_FLAGS}
          -c ${CMAKE_CURRENT_SOURCE_DIR}/ompfor4.c)
set_tests_properties(OMPANALYZE_schedule_dynamic_for PROPERTIES LABELS OMPANALYZE)

if(ENABLE-FORTRAN)
  add_test(
    NAME OMPANALYZE_schedule_default_do
    WORKING_DIRECTORY ${_omp_test_output_dir}
    COMMAND checkOmpAnalyzing
            --rex-check=schedule-default-do
            ${ROSE_OMP_ANALYZING_FLAGS}
            -I${CMAKE_SOURCE_DIR}/src/frontend/SageIII
            -c ${CMAKE_CURRENT_SOURCE_DIR}/fortran/ompdo-default.f)
  set_tests_properties(OMPANALYZE_schedule_default_do PROPERTIES LABELS OMPANALYZE)
endif()

add_test(
  NAME OMPANALYZE_target_implicit_map
  WORKING_DIRECTORY ${_omp_test_output_dir}
  COMMAND checkOmpAnalyzing
          --rex-check=target-implicit-map
          ${ROSE_OMP_ANALYZING_FLAGS}
          -c ${CMAKE_CURRENT_SOURCE_DIR}/analyzing_target_implicit_map.c)
set_tests_properties(OMPANALYZE_target_implicit_map PROPERTIES LABELS OMPANALYZE)

# Rodinia-derived OpenMP lowering regressions (separate from AST output refs).
add_subdirectory(lowering_rodinia)

# CPU lowering semantic equivalence (original vs lowered executable behavior).
add_subdirectory(lowering_cpu)
