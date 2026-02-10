add_executable(parseOmp parseOmp.C)
target_link_libraries(parseOmp ROSE_DLL ${link_with_libraries})

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

# C test codes - synced with Autotools REX_C_TESTCODES_REQUIRED_TO_PASS
set(C_TESTCODES
	3loops.c affinity.c allocate.c jacobi.c jacobi-collapse.c alignment.c
	array_init.c array_init_2.c atoms-2.c atomic.c atomic-3.c barrier.c
	cancel.c cancellation_point.c collapse.c copyin.c copyprivate2.c copyprivate3.c
	copyprivate.c critical.c critical1.c critical_dead.c critical_orphaned.c
	depobj.c task_dep1.c task_dep3.c task_dep_iterator1.c task_dep_iterator2.c
	declare_simd.c distribute_parallel_for.c distribute_parallel_for_simd.c
	distribute_simd.c distribute.c distribute2.c dijkstra_open_mp.c dynamicChunk.c
	empty.c endif.c endif2.c endif3.c nowait.c expressions.c falsesharing.c
	firstprivate.c firstPrivateArray.c firstPrivateArray_2.c firstlastprivate.c
	flush.c flush_exampleA_21_1c.c full_verify.c flush_with_clauses.c
	get_max_threads.c hello.c hello-1.c hello-2.c hello-ordered.c
	include_header_only.c init.c lastprivate0.c lastprivate.c lastprivateOrphaned.c
	limits_threads.c linebreak.c lockarray.c loop.c loop1.c lu_factorization.c
	master.c masterSingle.c mastertaskloop.c mastertaskloopsimd.c matrix_vector.c
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

# C++ test codes - synced with Autotools REX_CXX_TESTCODES_REQUIRED_TO_PASS
# Plus additional tests to track and fix with CFE
set(CXX_TESTCODES
	classMember.cpp hello1.cpp helloNested.cpp memberFunction.cpp
	memberFunction2.cpp objectPrivate.cpp objectFirstPrivate.cpp
	objectLastprivate.cpp referenceType.cpp
	orphanedAtomic.cpp preprocessingInfo2.cpp task_link.cpp task_link2.cpp
	task_tree.cpp jacobi-ompacc2.cpp data-member-access.cpp define-directive2.cpp)

set(TESTCODES ${C_TESTCODES} ${CXX_TESTCODES})

# OpenMP+OpenACC combined test codes - synced with Autotools REX_C_OMP_ACC_TESTCODES_REQUIRED_TO_PASS
set(OMP_ACC_TESTCODES
  axpy_ompacc.c axpy_ompacc2.c axpy_ompacc3.c
  matrixmultiply-ompacc.c matrixmultiply-ompacc2.c
  jacobi-ompacc.c jacobi-ompacc-v2.c jacobi-ompacc-opt1.c jacobi-ompacc-opt2.c
  jacobi-ompacc-multiGPU.c axpy_ompacc_mpi.c axpy_ompacc4.c
  axpy_ompacc_parseonly.c)

# ROSE flags for OpenMP parsing (CFE frontend only)
# C tests use C99-style declarations in for-loop initializers.
set(ROSE_C_FLAGS -std=c99 -rose:openmp:ast_only -w -rose:verbose 0)
set(ROSE_CXX_FLAGS -rose:openmp:ast_only -w -rose:verbose 0)

# Include path to find omp.h header
set(TESTCODE_INCLUDES -I${CMAKE_SOURCE_DIR}/src/frontend/SageIII)

foreach(file_to_test ${TESTCODES})
  if(file_to_test MATCHES "\\.c$")
    set(_rose_flags ${ROSE_C_FLAGS})
  else()
    set(_rose_flags ${ROSE_CXX_FLAGS})
  endif()
  add_test(
    NAME OMPTEST_${file_to_test}
    COMMAND parseOmp ${_rose_flags} ${TESTCODE_INCLUDES} -c
            ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})
  set_tests_properties(OMPTEST_${file_to_test} PROPERTIES LABELS OMPTEST)
endforeach()

foreach(file_to_test ${OMP_ACC_TESTCODES})
  add_test(
    NAME OMPACCTEST_${file_to_test}
    COMMAND parseOmp ${ROSE_C_FLAGS} -rose:skipfinalCompileStep
                     -c ${CMAKE_CURRENT_SOURCE_DIR}/${file_to_test})
  set_tests_properties(OMPACCTEST_${file_to_test} PROPERTIES LABELS OMPACCTEST)
endforeach()


# Multi-file test: verifies correct handling of multiple source files with OpenMP pragmas
add_test(
  NAME OMPTEST_bonds-2
  COMMAND parseOmp ${ROSE_C_FLAGS} ${TESTCODE_INCLUDES}
          -c ${CMAKE_CURRENT_SOURCE_DIR}/atoms-2.c
          ${CMAKE_CURRENT_SOURCE_DIR}/bonds-2.c)
set_tests_properties(OMPTEST_bonds-2 PROPERTIES LABELS OMPTEST)

# Test for macro ID handling with comment/directive collection
add_test(
  NAME OMPTEST_macroIds
  COMMAND parseOmp ${ROSE_C_FLAGS} ${TESTCODE_INCLUDES}
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
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND ${BASH_EXECUTABLE} -c
      "grep \"pragma omp\" ${_rose_file} > ${_output_file} || exit 0"
  )
  set_tests_properties(OMPTEST_output_${_name} PROPERTIES DEPENDS "${dependency}" LABELS "${label}")

  add_test(
    NAME OMPTEST_output_diff_${_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
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

# Makefile check targets.
add_test(
  NAME nonsmoke_functional_CompileTests_OpenMP_tests_check-c
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=OMPTEST_3loops.c\;OMPTEST_affinity.c\;OMPTEST_alignment.c\;OMPTEST_allocate.c\;OMPTEST_array_init.c\;OMPTEST_array_init_2.c\;OMPTEST_atomic-3.c\;OMPTEST_atomic.c\;OMPTEST_atoms-2.c\;OMPTEST_barrier.c\;OMPTEST_cancel.c\;OMPTEST_cancellation_point.c\;OMPTEST_collapse.c\;OMPTEST_copyin.c\;OMPTEST_copyprivate.c\;OMPTEST_copyprivate2.c\;OMPTEST_copyprivate3.c\;OMPTEST_critical.c\;OMPTEST_critical1.c\;OMPTEST_critical_dead.c\;OMPTEST_critical_orphaned.c\;OMPTEST_declare_simd.c\;OMPTEST_depobj.c\;OMPTEST_dijkstra_open_mp.c\;OMPTEST_distribute.c\;OMPTEST_distribute2.c\;OMPTEST_distribute_parallel_for.c\;OMPTEST_distribute_parallel_for_simd.c\;OMPTEST_distribute_simd.c\;OMPTEST_dynamicChunk.c\;OMPTEST_empty.c\;OMPTEST_endif.c\;OMPTEST_endif2.c\;OMPTEST_endif3.c\;OMPTEST_expressions.c\;OMPTEST_falsesharing.c\;OMPTEST_firstPrivateArray.c\;OMPTEST_firstPrivateArray_2.c\;OMPTEST_firstlastprivate.c\;OMPTEST_firstprivate.c\;OMPTEST_flush.c\;OMPTEST_flush_exampleA_21_1c.c\;OMPTEST_flush_with_clauses.c\;OMPTEST_full_verify.c\;OMPTEST_get_max_threads.c\;OMPTEST_hello-1.c\;OMPTEST_hello-2.c\;OMPTEST_hello-ordered.c\;OMPTEST_hello.c\;OMPTEST_include_header_only.c\;OMPTEST_init.c\;OMPTEST_jacobi-collapse.c\;OMPTEST_jacobi.c\;OMPTEST_lastprivate.c\;OMPTEST_lastprivate0.c\;OMPTEST_lastprivateOrphaned.c\;OMPTEST_limits_threads.c\;OMPTEST_linebreak.c\;OMPTEST_lockarray.c\;OMPTEST_loop.c\;OMPTEST_loop1.c\;OMPTEST_lu_factorization.c\;OMPTEST_master.c\;OMPTEST_masterSingle.c\;OMPTEST_mastertaskloop.c\;OMPTEST_mastertaskloopsimd.c\;OMPTEST_matrix_vector.c\;OMPTEST_matrixmultiply.c\;OMPTEST_md_open_mp.c\;OMPTEST_multiple_return.c\;OMPTEST_nestedpar.c\;OMPTEST_nestedpar1.c\;OMPTEST_nowait.c\;OMPTEST_omp1.c\;OMPTEST_ompGetNumThreads.c\;OMPTEST_omp_sections.c\;OMPTEST_ompfor-decremental.c\;OMPTEST_ompfor-default.c\;OMPTEST_ompfor-static.c\;OMPTEST_ompfor.c\;OMPTEST_ompfor10.c\;OMPTEST_ompfor2.c\;OMPTEST_ompfor3.c\;OMPTEST_ompfor4.c\;OMPTEST_ompfor5.c\;OMPTEST_ompfor6.c\;OMPTEST_ompfor8.c\;OMPTEST_ompfor9.c\;OMPTEST_ompfor_c99.c\;OMPTEST_ordered.c\;OMPTEST_ordered2.c\;OMPTEST_ordered3.c\;OMPTEST_ordered_depend.c\;OMPTEST_orphaned-directives.c\;OMPTEST_parallel-if-numthreads.c\;OMPTEST_parallel-if.c\;OMPTEST_parallel-numthreads.c\;OMPTEST_parallel-reduction.c\;OMPTEST_parallel-reduction2.c\;OMPTEST_parallel.c\;OMPTEST_parallel_allocate_predefined_modifier.c\;OMPTEST_parallel_allocate_user_modifier.c\;OMPTEST_parallel_if_parallel.c\;OMPTEST_parallel_pragma.c\;OMPTEST_parallelfor.c\;OMPTEST_parallelfor2.c\;OMPTEST_parallelloop.c\;OMPTEST_parallelmaster.c\;OMPTEST_parallelmastertaskloop.c\;OMPTEST_parallelmastertaskloopsimd.c\;OMPTEST_parallelsections.c\;OMPTEST_pi.c\;OMPTEST_preprocessingInfo.c\;OMPTEST_prettyfunc.c\;OMPTEST_private-duplicate.c\;OMPTEST_private.c\;OMPTEST_privatej.c\;OMPTEST_recursive.c\;OMPTEST_reduction-classic.c\;OMPTEST_reduction.c\;OMPTEST_reduction2.c\;OMPTEST_reduction_min_max.c\;OMPTEST_reduction_modifier.c\;OMPTEST_reduction_user_identifier.c\;OMPTEST_requires.c\;OMPTEST_requires_atomic_default_mem_order.c\;OMPTEST_requires_dynamic_allocators.c\;OMPTEST_requires_ext.c\;OMPTEST_requires_reverse_offload.c\;OMPTEST_requires_unified_address.c\;OMPTEST_requires_unified_shared_memory.c\;OMPTEST_rice1.c\;OMPTEST_scan.c\;OMPTEST_section.c\;OMPTEST_section1.c\;OMPTEST_sections.c\;OMPTEST_sections1.c\;OMPTEST_set_num_threads.c\;OMPTEST_shared.c\;OMPTEST_simd.c\;OMPTEST_simd2.c\;OMPTEST_simd3.c\;OMPTEST_simd4.c\;OMPTEST_simd5.c\;OMPTEST_single.c\;OMPTEST_single2.c\;OMPTEST_single3.c\;OMPTEST_single_copyprivate.c\;OMPTEST_sizeof.c\;OMPTEST_spmd1.c\;OMPTEST_staticChunk.c\;OMPTEST_subteam.c\;OMPTEST_subteam2.c\;OMPTEST_target.c\;OMPTEST_target1.c\;OMPTEST_target_update_from.c\;OMPTEST_target_update_to.c\;OMPTEST_target_uses_allocators.c\;OMPTEST_targetparallel.c\;OMPTEST_targetparallelforsimd.c\;OMPTEST_targetparallelloop.c\;OMPTEST_targetsimd.c\;OMPTEST_targetteams.c\;OMPTEST_targetteamsdistribute.c\;OMPTEST_targetteamsdistributeparallelfor.c\;OMPTEST_targetteamsdistributeparallelforsimd.c\;OMPTEST_targetteamsdistributesimd.c\;OMPTEST_targetteamsloop.c\;OMPTEST_targetupdate.c\;OMPTEST_targetupdate_device.c\;OMPTEST_targetupdate_nowait.c\;OMPTEST_task_affinity.c\;OMPTEST_task_affinity1.c\;OMPTEST_task_array.c\;OMPTEST_task_array2.c\;OMPTEST_task_dep.load.compute.c\;OMPTEST_task_dep1.c\;OMPTEST_task_dep2.c\;OMPTEST_task_dep3.c\;OMPTEST_task_dep_iterator1.c\;OMPTEST_task_dep_iterator2.c\;OMPTEST_task_final.c\;OMPTEST_task_largenumber.c\;OMPTEST_task_mergeable.c\;OMPTEST_task_orphaned.c\;OMPTEST_task_outlining.c\;OMPTEST_task_priority.c\;OMPTEST_task_underIf.c\;OMPTEST_task_untied.c\;OMPTEST_task_untied2.c\;OMPTEST_task_untied3.c\;OMPTEST_task_untied4.c\;OMPTEST_taskgroup.c\;OMPTEST_taskloop.c\;OMPTEST_taskloop_simd_reduction.1.c\;OMPTEST_taskwait.c\;OMPTEST_taskwait2.c\;OMPTEST_taskwait3.c\;OMPTEST_taskwait4.c\;OMPTEST_taskwait5.c\;OMPTEST_taskyield.c\;OMPTEST_teams.c\;OMPTEST_teamsdistribute.c\;OMPTEST_teamsdistributeparallelfor.c\;OMPTEST_teamsdistributeparallelforsimd.c\;OMPTEST_teamsdistributesimd.c\;OMPTEST_teamsloop.c\;OMPTEST_threadProcessor.c\;OMPTEST_threadprivate.c\;OMPTEST_threadprivate2.c\;OMPTEST_threadprivate3.c\;OMPTEST_twoRegions.c\;OMPTEST_upperCase.c\;OMPTEST_variables.c
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_OpenMP_tests_check-c PROPERTIES LABELS disabled DEPENDS "OMPTEST_3loops.c;OMPTEST_affinity.c;OMPTEST_alignment.c;OMPTEST_allocate.c;OMPTEST_array_init.c;OMPTEST_array_init_2.c;OMPTEST_atomic-3.c;OMPTEST_atomic.c;OMPTEST_atoms-2.c;OMPTEST_barrier.c;OMPTEST_cancel.c;OMPTEST_cancellation_point.c;OMPTEST_collapse.c;OMPTEST_copyin.c;OMPTEST_copyprivate.c;OMPTEST_copyprivate2.c;OMPTEST_copyprivate3.c;OMPTEST_critical.c;OMPTEST_critical1.c;OMPTEST_critical_dead.c;OMPTEST_critical_orphaned.c;OMPTEST_declare_simd.c;OMPTEST_depobj.c;OMPTEST_dijkstra_open_mp.c;OMPTEST_distribute.c;OMPTEST_distribute2.c;OMPTEST_distribute_parallel_for.c;OMPTEST_distribute_parallel_for_simd.c;OMPTEST_distribute_simd.c;OMPTEST_dynamicChunk.c;OMPTEST_empty.c;OMPTEST_endif.c;OMPTEST_endif2.c;OMPTEST_endif3.c;OMPTEST_expressions.c;OMPTEST_falsesharing.c;OMPTEST_firstPrivateArray.c;OMPTEST_firstPrivateArray_2.c;OMPTEST_firstlastprivate.c;OMPTEST_firstprivate.c;OMPTEST_flush.c;OMPTEST_flush_exampleA_21_1c.c;OMPTEST_flush_with_clauses.c;OMPTEST_full_verify.c;OMPTEST_get_max_threads.c;OMPTEST_hello-1.c;OMPTEST_hello-2.c;OMPTEST_hello-ordered.c;OMPTEST_hello.c;OMPTEST_include_header_only.c;OMPTEST_init.c;OMPTEST_jacobi-collapse.c;OMPTEST_jacobi.c;OMPTEST_lastprivate.c;OMPTEST_lastprivate0.c;OMPTEST_lastprivateOrphaned.c;OMPTEST_limits_threads.c;OMPTEST_linebreak.c;OMPTEST_lockarray.c;OMPTEST_loop.c;OMPTEST_loop1.c;OMPTEST_lu_factorization.c;OMPTEST_master.c;OMPTEST_masterSingle.c;OMPTEST_mastertaskloop.c;OMPTEST_mastertaskloopsimd.c;OMPTEST_matrix_vector.c;OMPTEST_matrixmultiply.c;OMPTEST_md_open_mp.c;OMPTEST_multiple_return.c;OMPTEST_nestedpar.c;OMPTEST_nestedpar1.c;OMPTEST_nowait.c;OMPTEST_omp1.c;OMPTEST_ompGetNumThreads.c;OMPTEST_omp_sections.c;OMPTEST_ompfor-decremental.c;OMPTEST_ompfor-default.c;OMPTEST_ompfor-static.c;OMPTEST_ompfor.c;OMPTEST_ompfor10.c;OMPTEST_ompfor2.c;OMPTEST_ompfor3.c;OMPTEST_ompfor4.c;OMPTEST_ompfor5.c;OMPTEST_ompfor6.c;OMPTEST_ompfor8.c;OMPTEST_ompfor9.c;OMPTEST_ompfor_c99.c;OMPTEST_ordered.c;OMPTEST_ordered2.c;OMPTEST_ordered3.c;OMPTEST_ordered_depend.c;OMPTEST_orphaned-directives.c;OMPTEST_parallel-if-numthreads.c;OMPTEST_parallel-if.c;OMPTEST_parallel-numthreads.c;OMPTEST_parallel-reduction.c;OMPTEST_parallel-reduction2.c;OMPTEST_parallel.c;OMPTEST_parallel_allocate_predefined_modifier.c;OMPTEST_parallel_allocate_user_modifier.c;OMPTEST_parallel_if_parallel.c;OMPTEST_parallel_pragma.c;OMPTEST_parallelfor.c;OMPTEST_parallelfor2.c;OMPTEST_parallelloop.c;OMPTEST_parallelmaster.c;OMPTEST_parallelmastertaskloop.c;OMPTEST_parallelmastertaskloopsimd.c;OMPTEST_parallelsections.c;OMPTEST_pi.c;OMPTEST_preprocessingInfo.c;OMPTEST_prettyfunc.c;OMPTEST_private-duplicate.c;OMPTEST_private.c;OMPTEST_privatej.c;OMPTEST_recursive.c;OMPTEST_reduction-classic.c;OMPTEST_reduction.c;OMPTEST_reduction2.c;OMPTEST_reduction_min_max.c;OMPTEST_reduction_modifier.c;OMPTEST_reduction_user_identifier.c;OMPTEST_requires.c;OMPTEST_requires_atomic_default_mem_order.c;OMPTEST_requires_dynamic_allocators.c;OMPTEST_requires_ext.c;OMPTEST_requires_reverse_offload.c;OMPTEST_requires_unified_address.c;OMPTEST_requires_unified_shared_memory.c;OMPTEST_rice1.c;OMPTEST_scan.c;OMPTEST_section.c;OMPTEST_section1.c;OMPTEST_sections.c;OMPTEST_sections1.c;OMPTEST_set_num_threads.c;OMPTEST_shared.c;OMPTEST_simd.c;OMPTEST_simd2.c;OMPTEST_simd3.c;OMPTEST_simd4.c;OMPTEST_simd5.c;OMPTEST_single.c;OMPTEST_single2.c;OMPTEST_single3.c;OMPTEST_single_copyprivate.c;OMPTEST_sizeof.c;OMPTEST_spmd1.c;OMPTEST_staticChunk.c;OMPTEST_subteam.c;OMPTEST_subteam2.c;OMPTEST_target.c;OMPTEST_target1.c;OMPTEST_target_update_from.c;OMPTEST_target_update_to.c;OMPTEST_target_uses_allocators.c;OMPTEST_targetparallel.c;OMPTEST_targetparallelforsimd.c;OMPTEST_targetparallelloop.c;OMPTEST_targetsimd.c;OMPTEST_targetteams.c;OMPTEST_targetteamsdistribute.c;OMPTEST_targetteamsdistributeparallelfor.c;OMPTEST_targetteamsdistributeparallelforsimd.c;OMPTEST_targetteamsdistributesimd.c;OMPTEST_targetteamsloop.c;OMPTEST_targetupdate.c;OMPTEST_targetupdate_device.c;OMPTEST_targetupdate_nowait.c;OMPTEST_task_affinity.c;OMPTEST_task_affinity1.c;OMPTEST_task_array.c;OMPTEST_task_array2.c;OMPTEST_task_dep.load.compute.c;OMPTEST_task_dep1.c;OMPTEST_task_dep2.c;OMPTEST_task_dep3.c;OMPTEST_task_dep_iterator1.c;OMPTEST_task_dep_iterator2.c;OMPTEST_task_final.c;OMPTEST_task_largenumber.c;OMPTEST_task_mergeable.c;OMPTEST_task_orphaned.c;OMPTEST_task_outlining.c;OMPTEST_task_priority.c;OMPTEST_task_underIf.c;OMPTEST_task_untied.c;OMPTEST_task_untied2.c;OMPTEST_task_untied3.c;OMPTEST_task_untied4.c;OMPTEST_taskgroup.c;OMPTEST_taskloop.c;OMPTEST_taskloop_simd_reduction.1.c;OMPTEST_taskwait.c;OMPTEST_taskwait2.c;OMPTEST_taskwait3.c;OMPTEST_taskwait4.c;OMPTEST_taskwait5.c;OMPTEST_taskyield.c;OMPTEST_teams.c;OMPTEST_teamsdistribute.c;OMPTEST_teamsdistributeparallelfor.c;OMPTEST_teamsdistributeparallelforsimd.c;OMPTEST_teamsdistributesimd.c;OMPTEST_teamsloop.c;OMPTEST_threadProcessor.c;OMPTEST_threadprivate.c;OMPTEST_threadprivate2.c;OMPTEST_threadprivate3.c;OMPTEST_twoRegions.c;OMPTEST_upperCase.c;OMPTEST_variables.c" )
add_test(
  NAME nonsmoke_functional_CompileTests_OpenMP_tests_check-cxx
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=OMPTEST_classMember.cpp\;OMPTEST_hello1.cpp\;OMPTEST_helloNested.cpp\;OMPTEST_jacobi-ompacc2.cpp\;OMPTEST_memberFunction.cpp\;OMPTEST_memberFunction2.cpp\;OMPTEST_objectFirstPrivate.cpp\;OMPTEST_objectLastprivate.cpp\;OMPTEST_objectPrivate.cpp\;OMPTEST_orphanedAtomic.cpp\;OMPTEST_preprocessingInfo2.cpp\;OMPTEST_referenceType.cpp\;OMPTEST_task_link.cpp\;OMPTEST_task_link2.cpp\;OMPTEST_task_tree.cpp
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_OpenMP_tests_check-cxx PROPERTIES LABELS disabled DEPENDS "OMPTEST_classMember.cpp;OMPTEST_hello1.cpp;OMPTEST_helloNested.cpp;OMPTEST_jacobi-ompacc2.cpp;OMPTEST_memberFunction.cpp;OMPTEST_memberFunction2.cpp;OMPTEST_objectFirstPrivate.cpp;OMPTEST_objectLastprivate.cpp;OMPTEST_objectPrivate.cpp;OMPTEST_orphanedAtomic.cpp;OMPTEST_preprocessingInfo2.cpp;OMPTEST_referenceType.cpp;OMPTEST_task_link.cpp;OMPTEST_task_link2.cpp;OMPTEST_task_tree.cpp" )
add_test(
  NAME nonsmoke_functional_CompileTests_OpenMP_tests_conditional-check-local
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_COMMAND}
    -DREX_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}
    -DREX_CTEST_BUILD_DIR=${CMAKE_BINARY_DIR}
    -DREX_CTEST_GROUP_TESTS=OMPACCTEST_axpy_ompacc.c\;OMPACCTEST_axpy_ompacc2.c\;OMPACCTEST_axpy_ompacc3.c\;OMPACCTEST_jacobi-ompacc-multiGPU.c\;OMPACCTEST_jacobi-ompacc-opt1.c\;OMPACCTEST_jacobi-ompacc-opt2.c\;OMPACCTEST_jacobi-ompacc-v2.c\;OMPACCTEST_jacobi-ompacc.c\;OMPACCTEST_matrixmultiply-ompacc.c\;OMPACCTEST_matrixmultiply-ompacc2.c\;OMPTEST_3loops.c\;OMPTEST_affinity.c\;OMPTEST_alignment.c\;OMPTEST_allocate.c\;OMPTEST_array_init.c\;OMPTEST_array_init_2.c\;OMPTEST_atomic-3.c\;OMPTEST_atomic.c\;OMPTEST_atoms-2.c\;OMPTEST_barrier.c\;OMPTEST_cancel.c\;OMPTEST_cancellation_point.c\;OMPTEST_classMember.cpp\;OMPTEST_collapse.c\;OMPTEST_copyin.c\;OMPTEST_copyprivate.c\;OMPTEST_copyprivate2.c\;OMPTEST_copyprivate3.c\;OMPTEST_critical.c\;OMPTEST_critical1.c\;OMPTEST_critical_dead.c\;OMPTEST_critical_orphaned.c\;OMPTEST_declare_simd.c\;OMPTEST_depobj.c\;OMPTEST_dijkstra_open_mp.c\;OMPTEST_distribute.c\;OMPTEST_distribute2.c\;OMPTEST_distribute_parallel_for.c\;OMPTEST_distribute_parallel_for_simd.c\;OMPTEST_distribute_simd.c\;OMPTEST_dynamicChunk.c\;OMPTEST_empty.c\;OMPTEST_endif.c\;OMPTEST_endif2.c\;OMPTEST_endif3.c\;OMPTEST_expressions.c\;OMPTEST_falsesharing.c\;OMPTEST_firstPrivateArray.c\;OMPTEST_firstPrivateArray_2.c\;OMPTEST_firstlastprivate.c\;OMPTEST_firstprivate.c\;OMPTEST_flush.c\;OMPTEST_flush_exampleA_21_1c.c\;OMPTEST_flush_with_clauses.c\;OMPTEST_full_verify.c\;OMPTEST_get_max_threads.c\;OMPTEST_hello-1.c\;OMPTEST_hello-2.c\;OMPTEST_hello-ordered.c\;OMPTEST_hello.c\;OMPTEST_hello1.cpp\;OMPTEST_helloNested.cpp\;OMPTEST_include_header_only.c\;OMPTEST_init.c\;OMPTEST_jacobi-collapse.c\;OMPTEST_jacobi-ompacc2.cpp\;OMPTEST_jacobi.c\;OMPTEST_lastprivate.c\;OMPTEST_lastprivate0.c\;OMPTEST_lastprivateOrphaned.c\;OMPTEST_limits_threads.c\;OMPTEST_linebreak.c\;OMPTEST_lockarray.c\;OMPTEST_loop.c\;OMPTEST_loop1.c\;OMPTEST_lu_factorization.c\;OMPTEST_master.c\;OMPTEST_masterSingle.c\;OMPTEST_mastertaskloop.c\;OMPTEST_mastertaskloopsimd.c\;OMPTEST_matrix_vector.c\;OMPTEST_matrixmultiply.c\;OMPTEST_md_open_mp.c\;OMPTEST_memberFunction.cpp\;OMPTEST_memberFunction2.cpp\;OMPTEST_multiple_return.c\;OMPTEST_nestedpar.c\;OMPTEST_nestedpar1.c\;OMPTEST_nowait.c\;OMPTEST_objectFirstPrivate.cpp\;OMPTEST_objectLastprivate.cpp\;OMPTEST_objectPrivate.cpp\;OMPTEST_omp1.c\;OMPTEST_ompGetNumThreads.c\;OMPTEST_omp_sections.c\;OMPTEST_ompfor-decremental.c\;OMPTEST_ompfor-default.c\;OMPTEST_ompfor-static.c\;OMPTEST_ompfor.c\;OMPTEST_ompfor10.c\;OMPTEST_ompfor2.c\;OMPTEST_ompfor3.c\;OMPTEST_ompfor4.c\;OMPTEST_ompfor5.c\;OMPTEST_ompfor6.c\;OMPTEST_ompfor8.c\;OMPTEST_ompfor9.c\;OMPTEST_ompfor_c99.c\;OMPTEST_ordered.c\;OMPTEST_ordered2.c\;OMPTEST_ordered3.c\;OMPTEST_ordered_depend.c\;OMPTEST_orphaned-directives.c\;OMPTEST_orphanedAtomic.cpp\;OMPTEST_parallel-if-numthreads.c\;OMPTEST_parallel-if.c\;OMPTEST_parallel-numthreads.c\;OMPTEST_parallel-reduction.c\;OMPTEST_parallel-reduction2.c\;OMPTEST_parallel.c\;OMPTEST_parallel_allocate_predefined_modifier.c\;OMPTEST_parallel_allocate_user_modifier.c\;OMPTEST_parallel_if_parallel.c\;OMPTEST_parallel_pragma.c\;OMPTEST_parallelfor.c\;OMPTEST_parallelfor2.c\;OMPTEST_parallelloop.c\;OMPTEST_parallelmaster.c\;OMPTEST_parallelmastertaskloop.c\;OMPTEST_parallelmastertaskloopsimd.c\;OMPTEST_parallelsections.c\;OMPTEST_pi.c\;OMPTEST_preprocessingInfo.c\;OMPTEST_preprocessingInfo2.cpp\;OMPTEST_prettyfunc.c\;OMPTEST_private-duplicate.c\;OMPTEST_private.c\;OMPTEST_privatej.c\;OMPTEST_recursive.c\;OMPTEST_reduction-classic.c\;OMPTEST_reduction.c\;OMPTEST_reduction2.c\;OMPTEST_reduction_min_max.c\;OMPTEST_reduction_modifier.c\;OMPTEST_reduction_user_identifier.c\;OMPTEST_referenceType.cpp\;OMPTEST_requires.c\;OMPTEST_requires_atomic_default_mem_order.c\;OMPTEST_requires_dynamic_allocators.c\;OMPTEST_requires_ext.c\;OMPTEST_requires_reverse_offload.c\;OMPTEST_requires_unified_address.c\;OMPTEST_requires_unified_shared_memory.c\;OMPTEST_rice1.c\;OMPTEST_scan.c\;OMPTEST_section.c\;OMPTEST_section1.c\;OMPTEST_sections.c\;OMPTEST_sections1.c\;OMPTEST_set_num_threads.c\;OMPTEST_shared.c\;OMPTEST_simd.c\;OMPTEST_simd2.c\;OMPTEST_simd3.c\;OMPTEST_simd4.c\;OMPTEST_simd5.c\;OMPTEST_single.c\;OMPTEST_single2.c\;OMPTEST_single3.c\;OMPTEST_single_copyprivate.c\;OMPTEST_sizeof.c\;OMPTEST_spmd1.c\;OMPTEST_staticChunk.c\;OMPTEST_subteam.c\;OMPTEST_subteam2.c\;OMPTEST_target.c\;OMPTEST_target1.c\;OMPTEST_target_update_from.c\;OMPTEST_target_update_to.c\;OMPTEST_target_uses_allocators.c\;OMPTEST_targetparallel.c\;OMPTEST_targetparallelforsimd.c\;OMPTEST_targetparallelloop.c\;OMPTEST_targetsimd.c\;OMPTEST_targetteams.c\;OMPTEST_targetteamsdistribute.c\;OMPTEST_targetteamsdistributeparallelfor.c\;OMPTEST_targetteamsdistributeparallelforsimd.c\;OMPTEST_targetteamsdistributesimd.c\;OMPTEST_targetteamsloop.c\;OMPTEST_targetupdate.c\;OMPTEST_targetupdate_device.c\;OMPTEST_targetupdate_nowait.c\;OMPTEST_task_affinity.c\;OMPTEST_task_affinity1.c\;OMPTEST_task_array.c\;OMPTEST_task_array2.c\;OMPTEST_task_dep.load.compute.c\;OMPTEST_task_dep1.c\;OMPTEST_task_dep2.c\;OMPTEST_task_dep3.c\;OMPTEST_task_dep_iterator1.c\;OMPTEST_task_dep_iterator2.c\;OMPTEST_task_final.c\;OMPTEST_task_largenumber.c\;OMPTEST_task_link.cpp\;OMPTEST_task_link2.cpp\;OMPTEST_task_mergeable.c\;OMPTEST_task_orphaned.c\;OMPTEST_task_outlining.c\;OMPTEST_task_priority.c\;OMPTEST_task_tree.cpp\;OMPTEST_task_underIf.c\;OMPTEST_task_untied.c\;OMPTEST_task_untied2.c\;OMPTEST_task_untied3.c\;OMPTEST_task_untied4.c\;OMPTEST_taskgroup.c\;OMPTEST_taskloop.c\;OMPTEST_taskloop_simd_reduction.1.c\;OMPTEST_taskwait.c\;OMPTEST_taskwait2.c\;OMPTEST_taskwait3.c\;OMPTEST_taskwait4.c\;OMPTEST_taskwait5.c\;OMPTEST_taskyield.c\;OMPTEST_teams.c\;OMPTEST_teamsdistribute.c\;OMPTEST_teamsdistributeparallelfor.c\;OMPTEST_teamsdistributeparallelforsimd.c\;OMPTEST_teamsdistributesimd.c\;OMPTEST_teamsloop.c\;OMPTEST_threadProcessor.c\;OMPTEST_threadprivate.c\;OMPTEST_threadprivate2.c\;OMPTEST_threadprivate3.c\;OMPTEST_twoRegions.c\;OMPTEST_upperCase.c\;OMPTEST_variables.c
    -P ${CMAKE_SOURCE_DIR}/scripts/run_ctest_group.cmake
)
set_tests_properties(nonsmoke_functional_CompileTests_OpenMP_tests_conditional-check-local PROPERTIES DEPENDS "OMPACCTEST_axpy_ompacc.c;OMPACCTEST_axpy_ompacc2.c;OMPACCTEST_axpy_ompacc3.c;OMPACCTEST_jacobi-ompacc-multiGPU.c;OMPACCTEST_jacobi-ompacc-opt1.c;OMPACCTEST_jacobi-ompacc-opt2.c;OMPACCTEST_jacobi-ompacc-v2.c;OMPACCTEST_jacobi-ompacc.c;OMPACCTEST_matrixmultiply-ompacc.c;OMPACCTEST_matrixmultiply-ompacc2.c;OMPTEST_3loops.c;OMPTEST_affinity.c;OMPTEST_alignment.c;OMPTEST_allocate.c;OMPTEST_array_init.c;OMPTEST_array_init_2.c;OMPTEST_atomic-3.c;OMPTEST_atomic.c;OMPTEST_atoms-2.c;OMPTEST_barrier.c;OMPTEST_cancel.c;OMPTEST_cancellation_point.c;OMPTEST_classMember.cpp;OMPTEST_collapse.c;OMPTEST_copyin.c;OMPTEST_copyprivate.c;OMPTEST_copyprivate2.c;OMPTEST_copyprivate3.c;OMPTEST_critical.c;OMPTEST_critical1.c;OMPTEST_critical_dead.c;OMPTEST_critical_orphaned.c;OMPTEST_declare_simd.c;OMPTEST_depobj.c;OMPTEST_dijkstra_open_mp.c;OMPTEST_distribute.c;OMPTEST_distribute2.c;OMPTEST_distribute_parallel_for.c;OMPTEST_distribute_parallel_for_simd.c;OMPTEST_distribute_simd.c;OMPTEST_dynamicChunk.c;OMPTEST_empty.c;OMPTEST_endif.c;OMPTEST_endif2.c;OMPTEST_endif3.c;OMPTEST_expressions.c;OMPTEST_falsesharing.c;OMPTEST_firstPrivateArray.c;OMPTEST_firstPrivateArray_2.c;OMPTEST_firstlastprivate.c;OMPTEST_firstprivate.c;OMPTEST_flush.c;OMPTEST_flush_exampleA_21_1c.c;OMPTEST_flush_with_clauses.c;OMPTEST_full_verify.c;OMPTEST_get_max_threads.c;OMPTEST_hello-1.c;OMPTEST_hello-2.c;OMPTEST_hello-ordered.c;OMPTEST_hello.c;OMPTEST_hello1.cpp;OMPTEST_helloNested.cpp;OMPTEST_include_header_only.c;OMPTEST_init.c;OMPTEST_jacobi-collapse.c;OMPTEST_jacobi-ompacc2.cpp;OMPTEST_jacobi.c;OMPTEST_lastprivate.c;OMPTEST_lastprivate0.c;OMPTEST_lastprivateOrphaned.c;OMPTEST_limits_threads.c;OMPTEST_linebreak.c;OMPTEST_lockarray.c;OMPTEST_loop.c;OMPTEST_loop1.c;OMPTEST_lu_factorization.c;OMPTEST_master.c;OMPTEST_masterSingle.c;OMPTEST_mastertaskloop.c;OMPTEST_mastertaskloopsimd.c;OMPTEST_matrix_vector.c;OMPTEST_matrixmultiply.c;OMPTEST_md_open_mp.c;OMPTEST_memberFunction.cpp;OMPTEST_memberFunction2.cpp;OMPTEST_multiple_return.c;OMPTEST_nestedpar.c;OMPTEST_nestedpar1.c;OMPTEST_nowait.c;OMPTEST_objectFirstPrivate.cpp;OMPTEST_objectLastprivate.cpp;OMPTEST_objectPrivate.cpp;OMPTEST_omp1.c;OMPTEST_ompGetNumThreads.c;OMPTEST_omp_sections.c;OMPTEST_ompfor-decremental.c;OMPTEST_ompfor-default.c;OMPTEST_ompfor-static.c;OMPTEST_ompfor.c;OMPTEST_ompfor10.c;OMPTEST_ompfor2.c;OMPTEST_ompfor3.c;OMPTEST_ompfor4.c;OMPTEST_ompfor5.c;OMPTEST_ompfor6.c;OMPTEST_ompfor8.c;OMPTEST_ompfor9.c;OMPTEST_ompfor_c99.c;OMPTEST_ordered.c;OMPTEST_ordered2.c;OMPTEST_ordered3.c;OMPTEST_ordered_depend.c;OMPTEST_orphaned-directives.c;OMPTEST_orphanedAtomic.cpp;OMPTEST_parallel-if-numthreads.c;OMPTEST_parallel-if.c;OMPTEST_parallel-numthreads.c;OMPTEST_parallel-reduction.c;OMPTEST_parallel-reduction2.c;OMPTEST_parallel.c;OMPTEST_parallel_allocate_predefined_modifier.c;OMPTEST_parallel_allocate_user_modifier.c;OMPTEST_parallel_if_parallel.c;OMPTEST_parallel_pragma.c;OMPTEST_parallelfor.c;OMPTEST_parallelfor2.c;OMPTEST_parallelloop.c;OMPTEST_parallelmaster.c;OMPTEST_parallelmastertaskloop.c;OMPTEST_parallelmastertaskloopsimd.c;OMPTEST_parallelsections.c;OMPTEST_pi.c;OMPTEST_preprocessingInfo.c;OMPTEST_preprocessingInfo2.cpp;OMPTEST_prettyfunc.c;OMPTEST_private-duplicate.c;OMPTEST_private.c;OMPTEST_privatej.c;OMPTEST_recursive.c;OMPTEST_reduction-classic.c;OMPTEST_reduction.c;OMPTEST_reduction2.c;OMPTEST_reduction_min_max.c;OMPTEST_reduction_modifier.c;OMPTEST_reduction_user_identifier.c;OMPTEST_referenceType.cpp;OMPTEST_requires.c;OMPTEST_requires_atomic_default_mem_order.c;OMPTEST_requires_dynamic_allocators.c;OMPTEST_requires_ext.c;OMPTEST_requires_reverse_offload.c;OMPTEST_requires_unified_address.c;OMPTEST_requires_unified_shared_memory.c;OMPTEST_rice1.c;OMPTEST_scan.c;OMPTEST_section.c;OMPTEST_section1.c;OMPTEST_sections.c;OMPTEST_sections1.c;OMPTEST_set_num_threads.c;OMPTEST_shared.c;OMPTEST_simd.c;OMPTEST_simd2.c;OMPTEST_simd3.c;OMPTEST_simd4.c;OMPTEST_simd5.c;OMPTEST_single.c;OMPTEST_single2.c;OMPTEST_single3.c;OMPTEST_single_copyprivate.c;OMPTEST_sizeof.c;OMPTEST_spmd1.c;OMPTEST_staticChunk.c;OMPTEST_subteam.c;OMPTEST_subteam2.c;OMPTEST_target.c;OMPTEST_target1.c;OMPTEST_target_update_from.c;OMPTEST_target_update_to.c;OMPTEST_target_uses_allocators.c;OMPTEST_targetparallel.c;OMPTEST_targetparallelforsimd.c;OMPTEST_targetparallelloop.c;OMPTEST_targetsimd.c;OMPTEST_targetteams.c;OMPTEST_targetteamsdistribute.c;OMPTEST_targetteamsdistributeparallelfor.c;OMPTEST_targetteamsdistributeparallelforsimd.c;OMPTEST_targetteamsdistributesimd.c;OMPTEST_targetteamsloop.c;OMPTEST_targetupdate.c;OMPTEST_targetupdate_device.c;OMPTEST_targetupdate_nowait.c;OMPTEST_task_affinity.c;OMPTEST_task_affinity1.c;OMPTEST_task_array.c;OMPTEST_task_array2.c;OMPTEST_task_dep.load.compute.c;OMPTEST_task_dep1.c;OMPTEST_task_dep2.c;OMPTEST_task_dep3.c;OMPTEST_task_dep_iterator1.c;OMPTEST_task_dep_iterator2.c;OMPTEST_task_final.c;OMPTEST_task_largenumber.c;OMPTEST_task_link.cpp;OMPTEST_task_link2.cpp;OMPTEST_task_mergeable.c;OMPTEST_task_orphaned.c;OMPTEST_task_outlining.c;OMPTEST_task_priority.c;OMPTEST_task_tree.cpp;OMPTEST_task_underIf.c;OMPTEST_task_untied.c;OMPTEST_task_untied2.c;OMPTEST_task_untied3.c;OMPTEST_task_untied4.c;OMPTEST_taskgroup.c;OMPTEST_taskloop.c;OMPTEST_taskloop_simd_reduction.1.c;OMPTEST_taskwait.c;OMPTEST_taskwait2.c;OMPTEST_taskwait3.c;OMPTEST_taskwait4.c;OMPTEST_taskwait5.c;OMPTEST_taskyield.c;OMPTEST_teams.c;OMPTEST_teamsdistribute.c;OMPTEST_teamsdistributeparallelfor.c;OMPTEST_teamsdistributeparallelforsimd.c;OMPTEST_teamsdistributesimd.c;OMPTEST_teamsloop.c;OMPTEST_threadProcessor.c;OMPTEST_threadprivate.c;OMPTEST_threadprivate2.c;OMPTEST_threadprivate3.c;OMPTEST_twoRegions.c;OMPTEST_upperCase.c;OMPTEST_variables.c" )
